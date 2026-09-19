// SPDX-License-Identifier: MPL-2.0

import type { StackTab } from "./stack-bar.tsx";
import type {
  ProxyDragEnvironment,
  ProxyDragService,
  ProxyDragTransaction,
  StackDragController,
} from "./types.ts";

export const PROXY_DRAG_END_EVENT = "floorp-stack-proxy-dragend";

/** Source-window ownership survives removal/adoption of the proxy DOM node. */
export class ProxyDragLifecycle {
  private current: ProxyDragTransaction | null = null;
  private timer: number | null = null;

  constructor(private readonly environment: ProxyDragEnvironment) {}

  begin(tab: StackTab, source: Element, controller: StackDragController): void {
    this.dispose();
    if (!tab._dragData) return;
    this.current = {
      tab,
      source,
      controller,
      data: tab._dragData,
      sawSession: false,
      dropped: false,
    };
    this.schedule();
  }

  noteDrop(tab: StackTab | null): void {
    if (this.current?.tab === tab) this.current.dropped = true;
  }

  end(tab: StackTab, event: DragEvent): void {
    const drag = this.current;
    if (!drag || drag.tab !== tab) return;
    this.dispose();
    try {
      // Native drops may already have finalized the payload. A late dragend
      // after recovery must not enter the native detach path a second time.
      if (tab._dragData === drag.data) drag.controller.handle_dragend(event);
    } finally {
      this.environment.finished();
    }
  }

  dispose(): void {
    if (this.timer !== null) this.environment.cancel(this.timer);
    this.timer = null;
    this.current = null;
  }

  private schedule(): void {
    this.timer = this.environment.schedule(() => this.check());
  }

  private check(): void {
    this.timer = null;
    const drag = this.current;
    if (!drag) return;
    let session: unknown;
    try {
      session = this.environment.readSession();
    } catch {
      // Failure to query the platform does not prove a live drag has ended.
      this.schedule();
      return;
    }
    if (session) drag.sawSession = true;
    if (
      session ||
      (!drag.dropped && drag.source.isConnected && !drag.sawSession &&
        drag.tab._dragData === drag.data)
    ) {
      this.schedule();
      return;
    }
    this.dispose();
    try {
      // Do not disturb a later native drag on the same tab. fromTabList avoids
      // strip animations, but native controller cleanup is still necessary.
      if (drag.tab._dragData !== drag.data) return;
      if (drag.tab.isConnected && !drag.tab.closing) {
        drag.controller.finishMoveTogetherSelectedTabs(drag.tab);
      }
      drag.controller.finishAnimateTabMove();
      drag.controller._resetTabsAfterDrop(drag.tab);
      if (drag.tab._dragData === drag.data) delete drag.tab._dragData;
    } catch (error) {
      console.error("[tab-stacks] Proxy drag recovery failed:", error);
    } finally {
      this.environment.finished();
    }
  }
}

export const proxyDragLifecycle = new ProxyDragLifecycle({
  readSession: () => {
    const service = Cc["@mozilla.org/widget/dragservice;1"].getService(
      Ci.nsIDragService,
    ) as unknown as ProxyDragService;
    return service.getCurrentSession(window);
  },
  schedule: (callback) => self.setTimeout(callback, 200),
  cancel: (timer) => self.clearTimeout(timer),
  finished: () => dispatchEvent(new Event(PROXY_DRAG_END_EVENT)),
});
