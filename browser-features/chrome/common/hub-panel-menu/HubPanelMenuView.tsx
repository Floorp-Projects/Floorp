// SPDX-License-Identifier: MPL-2.0

import { createEffect, createRoot, createSignal, onCleanup } from "solid-js";
import { render } from "@nora/solid-xul";
import type {
  HubPanelMenuFocusContext,
  HubPanelMenuViewOptions,
  OwnedHubPanelMenuView,
} from "./types.ts";

export const HUB_MENU_LABEL_FALLBACK = "Floorp Hub";

interface FocusClaim {
  readonly fallback: Element;
  readonly deadline: number;
  readonly cancel: () => void;
}

const focusClaims = new WeakMap<Element, FocusClaim>();
const focusClaimDurationMs = 3_000;

function isVisible(element: Element): boolean {
  if (
    !element.isConnected || element.hasAttribute("hidden") ||
    element.getAttribute("collapsed") === "true"
  ) return false;
  const view = element.ownerDocument.defaultView;
  if (!view) return false;
  const style = view.getComputedStyle(element);
  if (!style) return false;
  return style.display !== "none" && style.visibility !== "hidden" &&
    element.getClientRects().length > 0;
}

function focusElement(element: Element): boolean {
  if (!isVisible(element)) return false;
  const xulElement = element as Element & {
    disabled?: boolean;
    focus?: () => void;
  };
  if (xulElement.disabled || typeof xulElement.focus !== "function") {
    return false;
  }
  xulElement.focus();
  return element.ownerDocument.activeElement === element;
}

export function cancelHubMenuFocusClaim(trigger: Element): void {
  focusClaims.get(trigger)?.cancel();
}

function asError(error: unknown): Error {
  return error instanceof Error ? error : new Error(String(error));
}

function createFocusClaim(
  context: HubPanelMenuFocusContext,
  fallback: Element,
): FocusClaim {
  const document = context.trigger.ownerDocument;
  const window = document.defaultView;
  let timer = 0;
  let acceptingFallbackFocus = true;
  const observer = new MutationObserver(() => {
    if (!context.isMenuOpen()) claim.cancel();
  });
  const cancel = (): void => {
    if (focusClaims.get(context.trigger) === claim) {
      focusClaims.delete(context.trigger);
    }
    document.removeEventListener("focusin", onFocusIn, true);
    document.removeEventListener("keydown", onUserInput, true);
    document.removeEventListener("pointerdown", onUserInput, true);
    document.removeEventListener("pointermove", onUserInput, true);
    window?.removeEventListener("blur", onWindowBlur, true);
    observer.disconnect();
    if (timer) window?.clearTimeout(timer);
  };
  const onFocusIn = (event: FocusEvent): void => {
    if (
      acceptingFallbackFocus &&
      (document.activeElement === fallback ||
        event.composedPath().includes(fallback))
    ) {
      acceptingFallbackFocus = false;
      return;
    }
    cancel();
  };
  const onUserInput = (): void => cancel();
  const onWindowBlur = (event: FocusEvent): void => {
    if (event.target === window || event.target === document) cancel();
  };
  const claim: FocusClaim = {
    fallback,
    deadline: Date.now() + focusClaimDurationMs,
    cancel,
  };
  focusClaims.set(context.trigger, claim);
  document.addEventListener("focusin", onFocusIn, true);
  document.addEventListener("keydown", onUserInput, true);
  document.addEventListener("pointerdown", onUserInput, true);
  document.addEventListener("pointermove", onUserInput, true);
  window?.addEventListener("blur", onWindowBlur, true);
  observer.observe(context.trigger, {
    attributes: true,
    attributeFilter: ["open"],
  });
  timer = window?.setTimeout(cancel, focusClaimDurationMs) ?? 0;
  return claim;
}

function moveFocusToFallback(
  node: Element,
  context: HubPanelMenuFocusContext | undefined,
  allowClaim: boolean,
): FocusClaim | null {
  let claim: FocusClaim | null = null;
  try {
    const active = node.ownerDocument.activeElement;
    if (active !== node && !node.contains(active)) return null;
    if (context) cancelHubMenuFocusClaim(context.trigger);
    const fallback = context && isVisible(context.settings)
      ? context.settings
      : context && isVisible(context.trigger)
      ? context.trigger
      : null;
    if (!fallback) return null;
    claim = allowClaim && context && context.isForeground() &&
        context.isMenuOpen()
      ? createFocusClaim(context, fallback)
      : null;
    if (!focusElement(fallback)) {
      claim?.cancel();
      return null;
    }
    return claim;
  } catch (error) {
    const errors = [asError(error)];
    try {
      claim?.cancel();
    } catch (cancelError) {
      errors.push(asError(cancelError));
    }
    if (errors.length > 1) {
      throw new AggregateError(errors, "HUB menu focus transfer failed");
    }
    throw error;
  }
}

function restoreFocusClaim(
  node: Element,
  enabled: boolean,
  context: HubPanelMenuFocusContext | undefined,
): void {
  if (!context) return;
  const claim = focusClaims.get(context.trigger);
  if (!claim) return;
  const document = context.trigger.ownerDocument;
  if (
    Date.now() > claim.deadline || !context.isForeground() ||
    !context.isMenuOpen() || document.activeElement !== claim.fallback ||
    !isVisible(claim.fallback)
  ) {
    claim.cancel();
    return;
  }
  if (!enabled) return;
  if (!focusElement(node)) {
    claim.cancel();
    return;
  }
  claim.cancel();
}

export function observeHubMenuReadiness(
  root: Node,
  isReady: () => boolean,
  notify: () => void,
): () => void {
  const observer = new MutationObserver(() => {
    if (!isReady()) return;
    observer.disconnect();
    notify();
  });
  observer.observe(root, {
    childList: true,
    subtree: true,
    attributes: true,
    attributeFilter: ["open"],
  });
  return () => observer.disconnect();
}

export function mountOwnedHubPanelMenuView(
  options: HubPanelMenuViewOptions,
): OwnedHubPanelMenuView {
  const rootDisposer: { current: (() => void) | null } = { current: null };
  let node: (Element & { disabled: boolean; focus: () => void }) | null = null;
  let translationDisposer: (() => void) | null = null;
  let setEnabled: (enabled: boolean) => void = () => {};
  let setReactiveEnabled: (enabled: boolean) => void = () => {};
  let disposed = false;
  let hasBeenEnabled = options.enabled;

  const cleanup = (errors: Error[]): void => {
    try {
      translationDisposer?.();
    } catch (error) {
      errors.push(error instanceof Error ? error : new Error(String(error)));
    }
    try {
      rootDisposer.current?.();
    } catch (error) {
      errors.push(error instanceof Error ? error : new Error(String(error)));
    }
    if (node) {
      try {
        node.removeEventListener("command", options.onCommand);
      } catch (error) {
        errors.push(error instanceof Error ? error : new Error(String(error)));
      }
      try {
        node.remove();
      } catch (error) {
        errors.push(error instanceof Error ? error : new Error(String(error)));
      }
    }
  };

  const throwCleanupErrors = (errors: Error[], message: string): never => {
    const failure = new AggregateError(errors, message);
    console.error("[HubPanelMenu]", failure);
    throw failure;
  };

  const readLabel = (): string => {
    const label = options.readLabel(
      options.localizationKey,
      HUB_MENU_LABEL_FALLBACK,
    );
    if (typeof label !== "string" || label.trim().length === 0) {
      throw new Error("invalid HUB menu label");
    }
    return label;
  };

  try {
    createRoot((dispose) => {
      rootDisposer.current = dispose;
      const [title, setTitle] = createSignal(HUB_MENU_LABEL_FALLBACK);
      const [isEnabled, setIsEnabled] = createSignal(options.enabled);
      setReactiveEnabled = setIsEnabled;
      let translationsReady = false;

      setTitle(readLabel());
      translationDisposer = options.observeTranslations(() => {
        try {
          setTitle(readLabel());
        } catch (cause) {
          const error = cause instanceof Error
            ? cause
            : new Error(String(cause));
          if (!translationsReady) throw error;
          if (options.owner && options.reportFailure) {
            try {
              options.reportFailure(options.owner, error);
            } catch (reportError) {
              console.error("[HubPanelMenu]", reportError);
            }
          } else {
            console.error("[HubPanelMenu]", error);
          }
        }
      }) ?? null;
      translationsReady = true;

      render(
        () => {
          const element = (
            <xul:toolbarbutton
              id={options.buttonId}
              tabindex="-1"
              class="subviewbutton"
              label={title()}
            />
          ) as Element & { disabled: boolean; focus: () => void };
          node = element;
          element.addEventListener("command", options.onCommand);
          onCleanup(() =>
            element.removeEventListener("command", options.onCommand)
          );
          return element;
        },
        options.parent,
        { marker: options.marker ?? undefined },
      );

      createEffect(() => {
        if (node) node.disabled = !isEnabled();
      });
    });
  } catch (error) {
    const primary = error instanceof Error ? error : new Error(String(error));
    const errors: Error[] = [];
    cleanup(errors);
    if (errors.length) {
      throwCleanupErrors([primary, ...errors], "HUB menu view setup failed");
    }
    throw error;
  }

  if (!node || !rootDisposer.current) {
    const errors: Error[] = [];
    cleanup(errors);
    if (errors.length) throwCleanupErrors(errors, "HUB menu view setup failed");
    throw new Error("HUB menu view failed to create its owned button");
  }

  const button = node as Element & { disabled: boolean; focus: () => void };
  setEnabled = (enabled) => {
    if (disposed) return;
    if (!enabled && hasBeenEnabled && options.focusContext) {
      cancelHubMenuFocusClaim(options.focusContext.trigger);
    }
    hasBeenEnabled = enabled;
    setReactiveEnabled(enabled);
    button.disabled = !enabled;
    restoreFocusClaim(button, enabled, options.focusContext);
  };
  restoreFocusClaim(button, options.enabled, options.focusContext);

  return {
    node: button,
    setEnabled,
    dispose: (reason = "remove") => {
      if (disposed) return;
      disposed = true;
      const errors: Error[] = [];
      let claim: FocusClaim | null = null;
      try {
        claim = moveFocusToFallback(
          button,
          options.focusContext,
          reason === "replacement",
        );
      } catch (error) {
        errors.push(asError(error));
      }
      if (reason !== "replacement" && options.focusContext) {
        try {
          cancelHubMenuFocusClaim(options.focusContext.trigger);
        } catch (error) {
          errors.push(asError(error));
        }
      }
      cleanup(errors);
      if (errors.length) {
        try {
          claim?.cancel();
        } catch (error) {
          errors.push(asError(error));
        }
        if (options.focusContext) {
          try {
            cancelHubMenuFocusClaim(options.focusContext.trigger);
          } catch (error) {
            errors.push(asError(error));
          }
        }
        throwCleanupErrors(errors, "HUB menu view cleanup failed");
      }
    },
  };
}
