/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRPanelSidebarChild extends JSWindowActorChild {
  actorCreated() {
    const window = this.contentWindow;
    if (
      window?.location.port === "5183" ||
      window?.location.href.startsWith("chrome://noraneko-settings") ||
      window?.location.href.startsWith("about:")
    ) {
      Cu.exportFunction(this.GetContainerContexts.bind(this), window, {
        defineAs: "NRGetContainerContexts",
      });
      Cu.exportFunction(this.GetStaticPanels.bind(this), window, {
        defineAs: "NRGetStaticPanels",
      });
      Cu.exportFunction(this.GetExtensionPanels.bind(this), window, {
        defineAs: "NRGetExtensionPanels",
      });
    }
  }

  GetContainerContexts(callback: (containers: string) => void = () => {}) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveGetContainerContexts = resolve;
    });
    this.sendAsyncMessage("NRPanelSidebar:GetContainerContexts");
    promise.then((containers) => {
      callback(containers);
    });
  }

  resolveGetContainerContexts: ((containers: string) => void) | null = null;

  GetStaticPanels(callback: (panels: string) => void = () => {}) {
    const staticPanels = [
      {
        id: "floorp//bmt",
        title: "bookmark",
        icon: "chrome://browser/skin/bookmark.svg",
      },
      {
        id: "floorp//bookmarks",
        title: "bookmark",
        icon: "chrome://browser/skin/bookmark.svg",
      },
      {
        id: "floorp//history",
        title: "history",
        icon: "chrome://browser/skin/history.svg",
      },
      {
        id: "floorp//downloads",
        title: "downloads",
        icon: "chrome://browser/skin/downloads/downloads.svg",
      },
      {
        id: "floorp//notes",
        title: "notes",
        icon: "chrome://browser/skin/notes.svg",
      },
    ];
    const panelsStr = JSON.stringify(staticPanels);
    callback(panelsStr);
  }

  GetExtensionPanels(callback: (extensions: string) => void = () => {}) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveGetExtensionPanels = resolve;
    });
    this.sendAsyncMessage("NRPanelSidebar:GetExtensionPanels");
    promise.then((extensions) => {
      callback(extensions);
    });
  }

  resolveGetExtensionPanels: ((extensions: string) => void) | null = null;

  receiveMessage(message: { name: string; data: any }) {
    switch (message.name) {
      case "NRPanelSidebar:GetContainerContexts": {
        this.resolveGetContainerContexts?.(message.data);
        this.resolveGetContainerContexts = null;
        break;
      }
      case "NRPanelSidebar:GetExtensionPanels": {
        this.resolveGetExtensionPanels?.(message.data);
        this.resolveGetExtensionPanels = null;
        break;
      }
    }
  }
  handleEvent(_event: Event): void {
    // No-op
  }
}
