/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  NR_KEYBOARD_SHORTCUT_FOCUS_MESSAGE,
  type NRKeyboardShortcutFocusUpdate,
} from "../common/NRKeyboardShortcutFocusTypes.ts";

type EditableElement = Element & {
  isContentEditable?: boolean;
};

export function isKeyboardShortcutEditableTarget(
  target: EventTarget | null,
  designMode: string | null | undefined,
): boolean {
  if (designMode?.toLowerCase() === "on") {
    return true;
  }

  const element = target as EditableElement | null;
  const localName = element?.localName?.toLowerCase() ?? "";
  if (localName === "input" || localName === "textarea") {
    return true;
  }
  if (element?.isContentEditable === true) {
    return true;
  }

  let current: Element | null = element;
  while (current) {
    const value = current.getAttribute?.("contenteditable");
    if (value !== null && value !== undefined) {
      return value.toLowerCase() !== "false";
    }
    current = current.parentElement;
  }
  return false;
}

export function isKeyboardShortcutEditableFocusEvent(
  event: Event,
  doc: Document,
): boolean {
  const path = event.composedPath?.() ?? [];
  for (const target of path) {
    if (isKeyboardShortcutEditableTarget(target, doc.designMode)) {
      return true;
    }
  }
  return isKeyboardShortcutEditableTarget(doc.activeElement, doc.designMode);
}

export class NRKeyboardShortcutFocusChild extends JSWindowActorChild {
  private lastEditable = false;

  actorCreated(): void {
    this.publishCurrentFocus();
  }

  handleEvent(event: Event): void {
    const doc = this.contentWindow?.document;
    if (!doc) {
      this.publish(false);
      return;
    }

    switch (event.type) {
      case "focusin":
        this.publish(isKeyboardShortcutEditableFocusEvent(event, doc));
        break;
      case "DOMContentLoaded":
      case "pageshow":
        this.publishCurrentFocus();
        break;
      case "focusout":
      case "blur":
      case "pagehide":
        this.publish(false);
        break;
    }
  }

  willDestroy(): void {
    this.publish(false);
  }

  private publishCurrentFocus(): void {
    const doc = this.contentWindow?.document;
    this.publish(
      doc
        ? isKeyboardShortcutEditableTarget(doc.activeElement, doc.designMode)
        : false,
    );
  }

  private publish(editable: boolean): void {
    if (editable === this.lastEditable) {
      return;
    }
    this.lastEditable = editable;

    const data: NRKeyboardShortcutFocusUpdate = { editable };
    try {
      this.sendAsyncMessage(NR_KEYBOARD_SHORTCUT_FOCUS_MESSAGE, data);
    } catch (_error) {
      // Teardown can invalidate the actor between the event and this send.
    }
  }
}
