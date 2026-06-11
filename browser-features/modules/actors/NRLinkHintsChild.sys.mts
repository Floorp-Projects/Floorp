/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ElementDetector } from "./linkhints/ElementDetector.ts";
import { HintGenerator } from "./linkhints/HintGenerator.ts";
import { HintOverlay } from "./linkhints/HintOverlay.ts";
import { KeyboardHandler } from "./linkhints/KeyboardHandler.ts";
import type {
  LinkHintAction,
  ClickableElementInfo,
  LinkHintsActivateData,
  LinkHintsElementSelectedData,
} from "./linkhints/types.ts";
import { VALID_LINK_HINT_ACTIONS } from "./linkhints/types.ts";

export class NRLinkHintsChild extends JSWindowActorChild {
  private elementDetector: ElementDetector;
  private hintGenerator: HintGenerator;
  private hintOverlay: HintOverlay;
  private keyboardHandler: KeyboardHandler | null = null;
  private clickableElements: ClickableElementInfo[] = [];
  private typedChars = "";
  private currentAction: LinkHintAction = "openCurrentTab";
  private active = false;

  constructor() {
    super();
    this.elementDetector = new ElementDetector();
    this.hintGenerator = new HintGenerator();
    this.hintOverlay = new HintOverlay();
  }

  receiveMessage(message: { name: string; data?: unknown }): void {
    try {
      switch (message.name) {
        case "LinkHints:Activate": {
          const data = message.data as LinkHintsActivateData | undefined;
          const action =
            typeof data?.action === "string" &&
            (VALID_LINK_HINT_ACTIONS as readonly string[]).includes(data.action)
              ? (data.action as LinkHintAction)
              : "openCurrentTab";
          this.activate(action);
          break;
        }
        case "LinkHints:Deactivate": {
          this.deactivate();
          break;
        }
      }
    } catch (error) {
      console.error("[LinkHints]", error, { messageName: message.name, data: message.data });
    }
  }

  private activate(action: LinkHintAction): void {
    const win = this.contentWindow;
    const doc = this.document;
    if (!win || !doc) return;

    // Deactivate existing session if any
    this.deactivate();

    // Detect clickable elements
    this.clickableElements = this.elementDetector.detect(win);
    if (this.clickableElements.length === 0) {
      this.sendAsyncMessage("LinkHints:Cancelled", { reason: "noElements" });
      return;
    }

    this.currentAction = action;
    this.active = true;
    this.typedChars = "";

    // Generate and display hints
    const hints = this.hintGenerator.generate(this.clickableElements);
    this.hintOverlay.show(hints, doc);

    // Set up keyboard handler
    this.keyboardHandler = new KeyboardHandler({
      onCharacter: (char) => this.handleCharacter(char),
      onBackspace: () => this.handleBackspace(),
      onEscape: () => this.handleEscape(),
    });
    this.keyboardHandler.start(win);

    // Notify parent
    this.sendAsyncMessage("LinkHints:Shown", { count: hints.length });
  }

  private deactivate(): void {
    const doc = this.document;
    if (!doc) return;

    if (this.keyboardHandler) {
      this.keyboardHandler.stop(this.contentWindow);
      this.keyboardHandler = null;
    }

    this.hintOverlay.cleanup(doc);
    this.clickableElements = [];
    this.typedChars = "";
    this.active = false;
  }

  private handleCharacter(char: string): void {
    const newTyped = this.typedChars + char.toLowerCase();

    // Check if any hints match
    if (!this.hintOverlay.hasMatchingHints(newTyped)) {
      // No match - ignore this character
      return;
    }

    this.typedChars = newTyped;
    this.hintOverlay.filter(this.typedChars);

    // Check for exact match (complete hint)
    const matchedIndex = this.hintOverlay.getMatchedElementIndex(this.typedChars);
    if (matchedIndex !== undefined) {
      this.selectElement(matchedIndex);
    }
  }

  private handleBackspace(): void {
    if (this.typedChars.length === 0) {
      // Nothing to delete, do nothing
      return;
    }

    this.typedChars = this.typedChars.slice(0, -1);
    if (this.typedChars.length === 0) {
      // Show all hints again
      this.hintOverlay.filter("");
    } else {
      this.hintOverlay.filter(this.typedChars);
    }
  }

  private handleEscape(): void {
    this.sendAsyncMessage("LinkHints:Cancelled", { reason: "escape" });
    this.deactivate();
  }

  private selectElement(index: number): void {
    const el = this.clickableElements[index];
    if (!el) {
      this.deactivate();
      return;
    }

    // Handle hover entirely in the content process so we can dispatch
    // DOM events on the actual element without crossing the process boundary.
    if (this.currentAction === "hover") {
      this.dispatchHover(el.element);
      this.deactivate();
      return;
    }

    const data: LinkHintsElementSelectedData = {
      href: el.href,
      text: el.text,
      tagName: el.tagName,
      action: this.currentAction,
    };

    this.sendAsyncMessage("LinkHints:ElementSelected", data);
    this.deactivate();
  }

  /**
   * Dispatch hover-related mouse events on an element to simulate a hover.
   */
  private dispatchHover(element: HTMLElement): void {
    try {
      element.dispatchEvent(new MouseEvent("mouseover", { bubbles: true, cancelable: true }));
      element.dispatchEvent(new MouseEvent("mouseenter", { bubbles: true, cancelable: true }));
      if (typeof element.focus === "function") {
        element.focus();
      }
    } catch (error) {
      console.error("[LinkHints] Failed to dispatch hover events:", error);
    }
  }
}
