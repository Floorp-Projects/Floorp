// deno-lint-ignore-file
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRI18nChild extends JSWindowActorChild {
  actorCreated() {
    try {
      const window = this.contentWindow;
      if (!window) return;

      // Allow dev server and chrome/about privileged pages
      if (
        window.location.port === "5183" ||
        window.location.port === "5186" ||
        window.location.port === "5188" ||
        window.location.port === "5187" ||
        window.location.href.startsWith("chrome://") ||
        window.location.href.startsWith("about:")
      ) {
        // Create NRI18n API object on window
        const api = Cu.createObjectIn(window, { defineAs: "NRI18n" });

        // Helper to export async functions onto the API object
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

        // Expose getPrimaryBrowserLocaleMapped
        exportAsync("getPrimaryBrowserLocaleMapped", () => {
          return this.sendQuery("I18n:GetPrimaryLocale");
        });

        exportAsync("getOperatingSystemLocale", () => {
          return this.sendQuery("I18n:GetOSLocale");
        });

        exportAsync("normalizeLocale", (locale: string) => {
          return this.sendQuery("I18n:NormalizeLocale", locale);
        });

        // Expose GetConstants (returns JSON string of constants)
        exportAsync("getConstants", () => {
          return this.sendQuery("I18n:GetConstants");
        });

        // Expose register listener — child will ask parent to register and
        // will resolve when ack received. Listener callback will be forwarded
        // from parent via async messages. We'll provide addLocaleChangeListener
        // and removeLocaleChangeListener to mirror the parent utilities.

        // Internal map to store callbacks so we can call them when messages arrive
        this._localeListeners = new Set();

        // When parent forwards locale change messages, call registered listeners
        this.receiveMessage = (message) => {
          try {
            switch (message.name) {
              case "I18n:LocaleChanged": {
                const newLocale = message.data;
                for (const cb of this._localeListeners) {
                  try {
                    cb(newLocale);
                  } catch (e) {
                    // swallow callback errors
                    console.error("NRI18nChild listener error:", e);
                  }
                }
                break;
              }
              case "I18n:RegisterListener:Ack": {
                // resolve pending register promise
                if (this._pendingRegisterResolve) {
                  this._pendingRegisterResolve();
                  this._pendingRegisterResolve = null;
                }
                break;
              }
              case "I18n:RegisterListener:Error": {
                if (this._pendingRegisterReject) {
                  this._pendingRegisterReject(message.data);
                  this._pendingRegisterReject = null;
                }
                break;
              }
              case "I18n:UnregisterListener:Ack": {
                if (this._pendingUnregisterResolve) {
                  this._pendingUnregisterResolve();
                  this._pendingUnregisterResolve = null;
                }
                break;
              }
              case "I18n:UnregisterListener:Error": {
                if (this._pendingUnregisterReject) {
                  this._pendingUnregisterReject(message.data);
                  this._pendingUnregisterReject = null;
                }
                break;
              }
            }
          } catch (e) {
            console.error("Error in NRI18nChild.receiveMessage:", e);
          }
        };

        // Expose addLocaleChangeListener and removeLocaleChangeListener
        exportAsync(
          "addLocaleChangeListener",
          (cb: (newLocale: string) => void) => {
            // register in local set
            this._localeListeners.add(cb);
            // ask parent to register observer if this is the first
            if (this._localeListeners.size === 1) {
              return new Promise((resolve, reject) => {
                this._pendingRegisterResolve = resolve;
                this._pendingRegisterReject = reject;
                this.sendAsyncMessage("I18n:RegisterListener", null);
              });
            }
            return Promise.resolve();
          },
        );

        exportAsync(
          "removeLocaleChangeListener",
          (cb: (newLocale: string) => void) => {
            this._localeListeners.delete(cb);
            // if no listeners left, ask parent to unregister
            if (this._localeListeners.size === 0) {
              return new Promise((resolve, reject) => {
                this._pendingUnregisterResolve = resolve;
                this._pendingUnregisterReject = reject;
                this.sendAsyncMessage("I18n:UnregisterListener", null);
              });
            }
            return Promise.resolve();
          },
        );

        // API initialized
      }
    } catch (error) {
      console.error("[NRI18n Child] Error in actorCreated:", error);
    }
  }

  // receive messages from parent actor
  receiveMessage(message: ReceiveMessageArgument) {
    try {
      switch (message.name) {
        case "I18n:LocaleChanged": {
          const newLocale = message.data;
          for (const cb of this._localeListeners) {
            try {
              cb(newLocale);
            } catch (e) {
              // swallow callback errors
              console.error("NRI18nChild listener error:", e);
            }
          }
          break;
        }
        case "I18n:RegisterListener:Ack": {
          if (this._pendingRegisterResolve) {
            this._pendingRegisterResolve();
            this._pendingRegisterResolve = null;
          }
          break;
        }
        case "I18n:RegisterListener:Error": {
          if (this._pendingRegisterReject) {
            this._pendingRegisterReject(message.data);
            this._pendingRegisterReject = null;
          }
          break;
        }
        case "I18n:UnregisterListener:Ack": {
          if (this._pendingUnregisterResolve) {
            this._pendingUnregisterResolve();
            this._pendingUnregisterResolve = null;
          }
          break;
        }
        case "I18n:UnregisterListener:Error": {
          if (this._pendingUnregisterReject) {
            this._pendingUnregisterReject(message.data);
            this._pendingUnregisterReject = null;
          }
          break;
        }
      }
    } catch (e) {
      console.error("Error in NRI18nChild.receiveMessage:", e);
    }
  }

  // fields for internal tracking
  // deno-lint-ignore no-explicit-any
  _localeListeners: Set<any> = new Set();
  _pendingRegisterResolve: ((value?: unknown) => void) | null = null;
  _pendingRegisterReject: ((reason?: unknown) => void) | null = null;
  _pendingUnregisterResolve: ((value?: unknown) => void) | null = null;
  _pendingUnregisterReject: ((reason?: unknown) => void) | null = null;
}
