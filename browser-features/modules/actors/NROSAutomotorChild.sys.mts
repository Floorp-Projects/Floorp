// deno-lint-ignore-file
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NROSAutomotorChild extends JSWindowActorChild {
  actorCreated() {
    try {
      const window = this.contentWindow;
      if (!window) return;

      // Allow all localhost ports, chrome, and about pages
      const isLocalhost = window.location.hostname === "localhost" ||
                          window.location.hostname === "127.0.0.1";
      const isPrivileged = window.location.href.startsWith("chrome://") ||
                           window.location.href.startsWith("about:");

      if (isLocalhost || isPrivileged) {
        // Create OSAutomotor API object on window
        const api = Cu.createObjectIn(window, { defineAs: "OSAutomotor" });

        // Helper to export async functions
        const exportAsync = (
          name: string,
          // deno-lint-ignore no-explicit-any
          fn: (...args: any[]) => any,
        ) => {
          Cu.exportFunction(
            (...argsWrapped: unknown[]) => {
              return new window.Promise(
                (
                  resolve: (value: unknown) => void,
                  reject: (reason?: unknown) => void,
                ) => {
                  try {
                    const result = fn(...argsWrapped);
                    Promise.resolve(result).then(resolve, (err: unknown) => {
                      try {
                        if (err && typeof err === "object") {
                          // deno-lint-ignore no-explicit-any
                          const msg = (err as any).message ?? String(err);
                          reject(String(msg));
                        } else {
                          reject(
                            typeof err === "undefined"
                              ? "Unknown error"
                              : String(err),
                          );
                        }
                      } catch {
                        reject("Unknown error");
                      }
                    });
                  } catch (err) {
                    reject(
                      err && typeof err === "object"
                        ? // deno-lint-ignore no-explicit-any
                          String((err as any).message ?? err)
                        : String(err),
                    );
                  }
                },
              );
            },
            api,
            { defineAs: name },
          );
        };

        // Export isPlatformSupported (async)
        exportAsync("isPlatformSupported", () => {
          return this.sendQuery("OSAutomotor:IsPlatformSupported").then(
            (result: string) => result === "true",
          );
        });

        // Export isEnabled (async)
        exportAsync("isEnabled", () => {
          return this.sendQuery("OSAutomotor:IsEnabled").then(
            (result: string) => result === "true",
          );
        });

        // Export getInstalledVersion (async)
        exportAsync("getInstalledVersion", () => {
          return this.sendQuery("OSAutomotor:GetInstalledVersion");
        });

        // Export enable (async)
        exportAsync("enable", () => {
          return this.sendQuery("OSAutomotor:Enable").then((result: string) => {
            const parsed = JSON.parse(result);
            return Cu.cloneInto(parsed, window);
          });
        });

        // Export disable (async)
        exportAsync("disable", () => {
          return this.sendQuery("OSAutomotor:Disable").then(
            (result: string) => {
              const parsed = JSON.parse(result);
              return Cu.cloneInto(parsed, window);
            },
          );
        });

        // Export getStatus (async)
        exportAsync("getStatus", () => {
          return this.sendQuery("OSAutomotor:GetStatus").then(
            (result: string) => {
              const parsed = JSON.parse(result);
              return Cu.cloneInto(parsed, window);
            },
          );
        });

        // Export getPlatformDebugInfo (async)
        exportAsync("getPlatformDebugInfo", () => {
          return this.sendQuery("OSAutomotor:GetPlatformDebugInfo").then(
            (result: string) => {
              const parsed = JSON.parse(result);
              return Cu.cloneInto(parsed, window);
            },
          );
        });

        // Export sendWorkflowProgress (async) - for workflow progress window
        exportAsync("sendWorkflowProgress", (progressData: unknown) => {
          return this.sendQuery("OSAutomotor:WorkflowProgress", progressData).then(
            (result: string) => {
              const parsed = JSON.parse(result);
              return Cu.cloneInto(parsed, window);
            },
          );
        });

        // Export getWorkflowProgress (async) - for progress window to poll state
        exportAsync("getWorkflowProgress", () => {
          return this.sendQuery("OSAutomotor:GetWorkflowProgress").then(
            (result: string) => {
              const parsed = JSON.parse(result);
              return Cu.cloneInto(parsed, window);
            },
          );
        });

        // Export closeProgressWindow (async)
        exportAsync("closeProgressWindow", () => {
          return this.sendQuery("OSAutomotor:CloseProgressWindow").then(
            (result: string) => {
              const parsed = JSON.parse(result);
              return Cu.cloneInto(parsed, window);
            },
          );
        });

        console.info("[OSAutomotor] API initialized on window.OSAutomotor");
      }
    } catch (error) {
      console.error("[OSAutomotor Child] Error in actorCreated:", error);
    }
  }
  handleEvent(_event: Event): void {
    // No-op
  }
}
