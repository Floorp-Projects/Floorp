/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Import setTimeout
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

// Define a type for tab callback information
type TabCallbackInfo = {
  browsingContext: BrowsingContext | null; // Allow null
  registrationTime: number;
};

export class NRWebContentModifierParent extends JSWindowActorParent {
  private static tabCallbacks: Map<string, TabCallbackInfo> = new Map(); // Keep track of tabs expecting callbacks (like OneDrive)
  // No need for a separate map for Gmail callbacks in the parent, the ID is passed through

  receiveMessage(message: any) {
    console.log(
      `NRWebContentModifierParent: Received message: ${message.name}`,
    );

    switch (message.name) {
      case "WebContentModifier:ModificationApplied":
        console.log(
          "NRWebContentModifierParent: Content modification applied:",
          message.data,
        );

        try {
          const { url, status } = message.data;
          if (status === "success") {
            const win = Services.wm.getMostRecentWindow(
              "navigator:browser",
            ) as any;
            if (
              win?.PopupNotifications && this.browsingContext?.topFrameElement
            ) {
              win.PopupNotifications.show(
                this.browsingContext.topFrameElement,
                "floorp-content-modified",
                "Floorp.app content has been modified",
                "tab",
                null,
                null,
                {
                  persistent: false,
                  timeout: 3000,
                },
              );
            }
          }
        } catch (e) {
          console.error(
            "NRWebContentModifierParent: Error showing notification:",
            e,
          );
        }
        break;

      case "WebContentModifier:RegisterCallback":
        try {
          const { tabId } = message.data;
          console.log(
            `NRWebContentModifierParent: Registering callback for tab ${tabId}`,
          );

          NRWebContentModifierParent.tabCallbacks.set(tabId, {
            browsingContext: this.browsingContext,
            registrationTime: Date.now(),
          });

          console.log(
            `NRWebContentModifierParent: Total registered tabs: ${NRWebContentModifierParent.tabCallbacks.size}`,
          );
        } catch (e) {
          console.error(
            "NRWebContentModifierParent: Error registering callback:",
            e,
          );
        }
        break;

      case "WebContentModifier:OpenOneDrivePage":
        console.log(
          "NRWebContentModifierParent: Opening OneDrive page:",
          message.data,
        );
        this.openOneDrivePage(message.data.url, message.data.callbackTabId);
        break;

      case "WebContentModifier:SendFileDataToTab":
        try {
          const { callbackTabId, fileListJson } = message.data;
          console.log(
            `NRWebContentModifierParent: Sending file data to tab ${callbackTabId}`,
          );

          this.sendFileDataToTab(callbackTabId, fileListJson);
        } catch (e) {
          console.error(
            "NRWebContentModifierParent: Error sending file data to tab:",
            e,
          );
        }
        break;

      case "WebContentModifier:OneDriveFileListExtracted":
        console.log(
          "NRWebContentModifierParent: OneDrive file list extracted:",
          message.data,
        );
        try {
          const { count, fileList } = message.data;

          if (fileList && this.browsingContext && this.browsingContext.top) {
            for (
              const [tabId, tabInfo] of NRWebContentModifierParent.tabCallbacks
                .entries()
            ) {
              const isRecent = (Date.now() - tabInfo.registrationTime) < 60000;
              if (isRecent) {
                console.log(
                  `NRWebContentModifierParent: Trying to send data to registered tab ${tabId}`,
                );
                this.sendFileDataToTab(tabId, JSON.stringify(fileList));
              }
            }
          }
        } catch (e) {
          console.error(
            "NRWebContentModifierParent: Error showing notification:",
            e,
          );
        }
        break;

      // Handle request to open Gmail compose window
      case "WebContentModifier:OpenGmailCompose":
        console.log(
          "NRWebContentModifierParent: Received OpenGmailCompose message",
          message.data,
        );
        this.openGmailCompose(
          message.data.to,
          message.data.subject,
          message.data.body,
          message.data.callbackId,
        );
        break;

      // Handle the result from the Gmail tab
      case "WebContentModifier:GmailSent":
        console.log(
          "NRWebContentModifierParent: Received GmailSent message",
          message.data,
        );
        this.forwardGmailResult(
          message.data.callbackId,
          message.data.result,
        );
        break;
    }
    return null;
  }

  sendFileDataToTab(tabId: string, fileListJson: string): void {
    try {
      const tabInfo = NRWebContentModifierParent.tabCallbacks.get(tabId);
      if (!tabInfo || !tabInfo.browsingContext) {
        console.error(
          `NRWebContentModifierParent: Tab ${tabId} not found or invalid`,
        );
        return;
      }

      console.log(
        `NRWebContentModifierParent: Found tab ${tabId}, sending data`,
      );

      try {
        // @ts-ignore - Suppress error for currentWindowGlobal access
        if (!tabInfo.browsingContext.currentWindowGlobal) {
          console.error(
            `NRWebContentModifierParent: Tab ${tabId} has no currentWindowGlobal`,
          );
          return;
        }

        // @ts-ignore - Suppress error for currentWindowGlobal access
        const actor = tabInfo.browsingContext.currentWindowGlobal.getActor(
          "NRWebContentModifier",
        );

        actor.sendAsyncMessage("WebContentModifier:FileDataForCallback", {
          fileListJson,
        });

        console.log(
          `NRWebContentModifierParent: Sent file data to tab ${tabId}`,
        );
      } catch (sendError) {
        console.error(
          `NRWebContentModifierParent: Error sending to tab ${tabId}:`,
          sendError,
        );
      }
    } catch (e) {
      console.error(
        "NRWebContentModifierParent: Error in sendFileDataToTab:",
        e,
      );
    }
  }

  async openOneDrivePage(url: string, callbackTabId?: string) {
    try {
      console.log(
        `NRWebContentModifierParent: Opening OneDrive page: ${url} (callback: ${
          callbackTabId || "none"
        })`,
      );

      const window = Services.wm.getMostRecentWindow(
        "navigator:browser",
      ) as any;
      if (!window) {
        console.error("NRWebContentModifierParent: No browser window found");
        return;
      }

      const registerCallbackToNewTab = (newTab: any, tabId: string) => {
        setTimeout(() => {
          try {
            const browserContext = newTab.linkedBrowser?.browsingContext;
            // @ts-ignore - Suppress error for currentWindowGlobal access
            if (browserContext && browserContext.currentWindowGlobal) {
              // @ts-ignore - Suppress error for currentWindowGlobal access
              const actor = browserContext.currentWindowGlobal.getActor(
                "NRWebContentModifier",
              );
              actor.sendAsyncMessage(
                "WebContentModifier:RegisterCallbackTabId",
                {
                  tabId: tabId,
                },
              );
              console.log(
                `NRWebContentModifierParent: First attempt to register callback tab ID ${tabId} (2秒後)`,
              );
            }
          } catch (error) {
            console.log(
              `NRWebContentModifierParent: First registration attempt failed: ${error}`,
            );
          }

          setTimeout(() => {
            try {
              const browserContext = newTab.linkedBrowser?.browsingContext;
              // @ts-ignore - Suppress error for currentWindowGlobal access
              if (browserContext && browserContext.currentWindowGlobal) {
                // @ts-ignore - Suppress error for currentWindowGlobal access
                const actor = browserContext.currentWindowGlobal.getActor(
                  "NRWebContentModifier",
                );
                actor.sendAsyncMessage(
                  "WebContentModifier:RegisterCallbackTabId",
                  {
                    tabId: tabId,
                  },
                );
                console.log(
                  `NRWebContentModifierParent: Second attempt to register callback tab ID ${tabId} (5秒後)`,
                );
              }
            } catch (error) {
              console.log(
                `NRWebContentModifierParent: Second registration attempt failed: ${error}`,
              );
            }
          }, 3000);
        }, 2000);
      };

      try {
        const browser = window.gBrowser;
        const newtab = browser.addTab(url, {
          inBackground: true,
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
        });

        console.log("NRWebContentModifierParent: Opened OneDrive in new tab");

        if (callbackTabId) {
          registerCallbackToNewTab(newtab, callbackTabId);
        }

        return;
      } catch (error) {
        console.error(
          "NRWebContentModifierParent: Error opening tab:",
          error,
        );
      }
    } catch (e) {
      console.error(
        "NRWebContentModifierParent: Error opening OneDrive page:",
        e,
      );
    }
  }

  async checkAndModifyFloorpWebsite(browser: any) {
    try {
      const context = browser.browsingContext;
      // @ts-ignore - Suppress error for currentWindowGlobal access
      if (!context || !context.currentWindowGlobal) {
        console.log(
          "NRWebContentModifierParent: Browser context not available",
        );
        return null;
      }

      // @ts-ignore - Suppress error for currentWindowGlobal access
      const actor = context.currentWindowGlobal.getActor(
        "NRWebContentModifier",
      );
      const currentUrl = browser.currentURI.spec;

      console.log("NRWebContentModifierParent: Current URL:", currentUrl);

      if (currentUrl.includes("floorp.app")) {
        console.log(
          "NRWebContentModifierParent: Floorp website is opened:",
          currentUrl,
        );
        return actor.sendQuery("WebContentModifier:ModifyFloorpHeading");
      }

      if (currentUrl.includes("onedrive.live.com")) {
        console.log(
          "NRWebContentModifierParent: OneDrive website is opened:",
          currentUrl,
        );
        return actor.sendQuery("WebContentModifier:ExtractOneDriveFileList");
      }
    } catch (e) {
      console.error(
        "NRWebContentModifierParent: Error in checkAndModifyFloorpWebsite:",
        e,
      );
    }
    return null;
  }

  // Opens Gmail compose in a new tab and tells it to fill/send
  async openGmailCompose(
    to: string,
    subject: string,
    body: string,
    callbackId: string,
  ) {
    try {
      console.log(
        `NRWebContentModifierParent: Opening Gmail compose (callbackId: ${callbackId})`,
      );
      const gmailUrl = "https://mail.google.com/mail/u/0/#inbox?compose=new";

      const window = Services.wm.getMostRecentWindow(
        "navigator:browser",
      ) as any;
      if (!window) {
        console.error("NRWebContentModifierParent: No browser window found");
        // Optionally send failure back via callbackId? Difficult without original tab context here.
        return;
      }

      const browser = window.gBrowser;
      const newtab = browser.addTab(gmailUrl, {
        inBackground: true, // Open in background
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      });

      console.log(
        "NRWebContentModifierParent: Opened Gmail compose in new tab",
      );

      // Need to wait for the tab's actor to be ready
      // We'll use a timeout loop, similar to OneDrive callback registration
      const attemptToSendToGmailTab = (attempt = 1) => {
        if (attempt > 10) { // Limit retries
          console.error(
            "NRWebContentModifierParent: Failed to get actor for Gmail tab after multiple attempts.",
          );
          // Send failure back to the original tab
          this.forwardGmailResult(callbackId, {
            success: false,
            message: "Failed to communicate with Gmail tab",
          });
          return;
        }

        try {
          // Check if context and global exist
          const browserContext = newtab.linkedBrowser?.browsingContext;
          if (browserContext) {
            // Assign to variable and use type assertion
            // @ts-ignore - Suppress error for currentWindowGlobal access
            const global = browserContext.currentWindowGlobal;
            if (global) {
              // @ts-ignore - Suppress error for getActor access
              const actor = global.getActor("NRWebContentModifier");

              if (actor) {
                console.log(
                  `NRWebContentModifierParent: Sending FillAndSendGmail to new tab (attempt ${attempt})`,
                );
                actor.sendAsyncMessage("WebContentModifier:FillAndSendGmail", {
                  to,
                  subject,
                  body,
                  callbackId,
                });
                return; // Success
              }
            }
          }
        } catch (error) {
          console.warn(
            `NRWebContentModifierParent: Error getting actor/sending message (attempt ${attempt}):`,
            error,
          );
        }

        // If actor not ready or error occurred, retry after a delay
        setTimeout(() => attemptToSendToGmailTab(attempt + 1), 1000 * attempt);
      };

      // Start the first attempt after a short delay
      setTimeout(() => attemptToSendToGmailTab(), 1000);
    } catch (error) {
      console.error(
        "NRWebContentModifierParent: Error opening Gmail compose:",
        error,
      );
      this.forwardGmailResult(callbackId, {
        success: false,
        message: `Error opening Gmail tab: ${
          error instanceof Error ? error.message : String(error)
        }`,
      });
    }
  }

  // Forwards the result of the Gmail send operation back to the original tab
  forwardGmailResult(
    callbackId: string,
    result: { success: boolean; message?: string },
  ): void {
    console.log(
      `NRWebContentModifierParent: Forwarding Gmail result for callbackId: ${callbackId}`,
    );

    // Find the original tab using the tabCallbacks map (used for OneDrive, but concept applies)
    // We actually don't *need* the browsingContext here if we just iterate and send
    // but finding the specific tab might be slightly more efficient if the map is large.

    let found = false;
    for (const tabInfo of NRWebContentModifierParent.tabCallbacks.values()) {
      // Check if context and global exist
      if (tabInfo.browsingContext) {
        // Assign to variable and use type assertion
        // @ts-ignore - Suppress error for currentWindowGlobal access
        const global = tabInfo.browsingContext.currentWindowGlobal;
        if (global) {
          try {
            // @ts-ignore - Suppress error for getActor access
            const actor = global.getActor("NRWebContentModifier");
            // Send the result, the child actor will check if the callbackId matches its stored ones
            actor.sendAsyncMessage("WebContentModifier:GmailResult", {
              callbackId,
              result,
            });
            // Assume only one tab should have this callbackId registered
            // We could potentially track the specific tab associated with the gmail callbackId
            // in NRSendGmail if needed, but sending to all registered tabs works for now.
            console.log(
              `NRWebContentModifierParent: Sent GmailResult to a potential originating tab.`,
            );
            found = true;
            // We potentially break here if we are sure only one tab originated the request
            // break;
          } catch (e) {
            console.warn(
              "NRWebContentModifierParent: Error sending GmailResult to a tab:",
              e,
            );
          }
        }
      }
    }

    if (!found) {
      console.warn(
        `NRWebContentModifierParent: Could not find any active tab context in tabCallbacks to forward Gmail result for ${callbackId}. The originating tab might be closed.`,
      );
      // If the original tab is closed, there's nothing more to do.
    }
  }
}
