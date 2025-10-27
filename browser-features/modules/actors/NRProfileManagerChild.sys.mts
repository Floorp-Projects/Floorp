/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

export class NRProfileManagerChild extends JSWindowActorChild {
  actorCreated() {
    const window = this.contentWindow;
    if (
      window?.location.port === "5179" ||
      window?.location.port === "5183" ||
      window?.location.href.startsWith("chrome://") ||
      window?.location.href.startsWith("about:")
    ) {
      // Export a set of helper functions to content window.
      const exports = [
        "NRGetCurrentProfile",
        "NRGetFxAccountsInfo",
        "NROpenUrl",
        "NRGetProfiles",
        "NROpenProfile",
        "NRCreateProfileWizard",
        "NRFlushProfiles",
        "NRRemoveProfile",
        "NRRenameProfile",
        "NRSetDefaultProfile",
        "NRRestart",
      ];

      for (const name of exports) {
        // Access methods dynamically via a typed lookup to avoid `any`.
        const lookup = this as unknown as Record<
          string,
          (...args: unknown[]) => unknown
        >;
        if (typeof lookup[name] === "function") {
          try {
            Cu.exportFunction(lookup[name].bind(this), window, {
              defineAs: name,
            });
          } catch (e) {
            console.error(
              "[NRProfileManagerChild] failed to export function:",
              name,
              e,
            );
          }
        }
      }
    }
  }

  // Simple request/response helper that supports concurrent requests.
  private _nextRequestId = 1;
  private _pending = new Map<
    number,
    { resolve: (v: unknown) => void; reject: (e: unknown) => void }
  >();

  private _sendRequest(
    messageName: string,
    payload: Record<string, unknown> = {},
  ): Promise<unknown> {
    const id = this._nextRequestId++;
    return new Promise((resolve, reject) => {
      this._pending.set(id, { resolve, reject });
      try {
        this.sendAsyncMessage(messageName, Object.assign({ id }, payload));
      } catch (e) {
        this._pending.delete(id);
        console.error(
          "[NRProfileManagerChild] _sendRequest: sendAsyncMessage failed",
          e,
        );
        reject(e);
      }
      // timeout after 10s
      setTimeout(() => {
        if (this._pending.has(id)) {
          this._pending
            .get(id)!
            .reject(new Error("NRProfileManager request timed out"));
          this._pending.delete(id);
        }
      }, 10000);
    });
  }

  // Exported functions used by content pages.
  NRGetCurrentProfile(callback: (profile: unknown) => void = () => {}) {
    this._sendRequest("NRProfileManager:GetCurrentProfile")
      .then((res) => {
        try {
          // Clone the result into the content window's compartment so the
          // content page receives a plain JS object (not a privileged/Restricted
          // wrapper). This avoids security wrapper issues when the value
          // crosses chrome/content boundary.
          const contentWin = this.contentWindow;
          const out =
            contentWin && typeof Cu?.cloneInto === "function"
              ? Cu.cloneInto(res, contentWin)
              : res;
          callback(out);
        } catch (e) {
          console.error(
            "[NRProfileManagerChild] NRGetCurrentProfile: cloneInto failed, falling back to raw",
            e,
          );
          callback(res);
        }
      })
      .catch((e) => {
        console.error("[NRProfileManagerChild] NRGetCurrentProfile: failed", e);
        callback(null);
      });
  }

  NRGetFxAccountsInfo(callback: (info: unknown) => void = () => {}) {
    this._sendRequest("NRProfileManager:GetFxAccountsInfo")
      .then((res) => {
        try {
          const contentWin = this.contentWindow;
          const out =
            contentWin && typeof Cu?.cloneInto === "function"
              ? Cu.cloneInto(res, contentWin)
              : res;
          callback(out);
        } catch (e) {
          console.error(
            "[NRProfileManagerChild] NRGetFxAccountsInfo: cloneInto failed, falling back to raw",
            e,
          );
          callback(res);
        }
      })
      .catch((e) => {
        console.error("[NRProfileManagerChild] NRGetFxAccountsInfo: failed", e);
        callback(null);
      });
  }

  NROpenUrl(url: string) {
    // fire-and-forget
    this.sendAsyncMessage("NRProfileManager:OpenUrl", { url });
  }

  NRGetProfiles(callback: (profiles: unknown[]) => void = () => {}) {
    this._sendRequest("NRProfileManager:GetProfiles")
      .then((res) => {
        try {
          const contentWin = this.contentWindow;
          const out =
            contentWin && typeof Cu?.cloneInto === "function"
              ? Cu.cloneInto(res, contentWin)
              : res;

          callback(Array.isArray(out) ? (out as unknown[]) : []);
        } catch (e) {
          console.error(
            "[NRProfileManagerChild] NRGetProfiles: cloneInto failed, falling back to raw",
            e,
          );
          callback(Array.isArray(res) ? (res as unknown[]) : []);
        }
      })
      .catch((e) => {
        console.error("[NRProfileManagerChild] NRGetProfiles: failed", e);
        callback([]);
      });
  }

  NROpenProfile(profileIdentifier: string) {
    this.sendAsyncMessage("NRProfileManager:OpenProfile", {
      profileIdentifier,
    });
  }

  NRCreateProfileWizard() {
    this.sendAsyncMessage("NRProfileManager:CreateProfileWizard");
  }

  NRFlushProfiles() {
    this.sendAsyncMessage("NRProfileManager:FlushProfiles");
  }

  NRRemoveProfile(
    profileIdentifier: string,
    deleteFiles: boolean = false,
    callback: (ok: boolean) => void = () => {},
  ) {
    this._sendRequest("NRProfileManager:RemoveProfile", {
      profileIdentifier,
      deleteFiles,
    })
      .then((res) => callback(Boolean(res)))
      .catch(() => callback(false));
  }

  NRRenameProfile(
    profileIdentifier: string,
    newName: string,
    callback: (ok: boolean) => void = () => {},
  ) {
    this._sendRequest("NRProfileManager:RenameProfile", {
      profileIdentifier,
      newName,
    })
      .then((res) => callback(Boolean(res)))
      .catch(() => callback(false));
  }

  NRSetDefaultProfile(
    profileIdentifier: string,
    callback: (ok: boolean) => void = () => {},
  ) {
    this._sendRequest("NRProfileManager:SetDefaultProfile", {
      profileIdentifier,
    })
      .then((res) => callback(Boolean(res)))
      .catch(() => callback(false));
  }

  NRRestart(safeMode: boolean = false) {
    this.sendAsyncMessage("NRProfileManager:Restart", { safeMode });
  }

  receiveMessage(message: ReceiveMessageArgument) {
    // Try to handle both old-style responses (same-name) and new style with id/result.
    try {
      if (
        message &&
        message.data &&
        typeof message.data === "object" &&
        "id" in message.data
      ) {
        const id = message.data.id as number;

        const pending = this._pending.get(id);
        if (pending) {
          this._pending.delete(id);
          if (message.data && message.data.error) {
            console.error(
              "[NRProfileManagerChild] receiveMessage: response contains error for id=",
              id,
              message.data.error,
            );
            pending.reject(message.data.error);
          } else {
            // accept both JSON strings and structured objects
            const result =
              typeof message.data.result === "string"
                ? safeParseJSON(message.data.result)
                : message.data.result;
            // If the result was stringified by the parent, safeParseJSON
            // returns a plain JS object which is safe to pass to content.

            pending.resolve(result);
          }
        }
        return;
      }

      switch (message.name) {
        case "NRProfileManager:GetCurrentProfile": {
          // backward compatibility: old code sent the payload directly
          const data = message.data;
          let parsed = data;
          if (typeof data === "string") {
            parsed = safeParseJSON(data);
          }

          // resolve any pending request without id if present (old pattern)
          // Find first pending and resolve
          const first = this._pending.values().next();
          if (!first.done) {
            // 'parsed' is already the parsed/plain value (safeParseJSON applied
            // above), so resolve with that plain value.
            first.value.resolve(parsed);
            // remove that pending entry (we don't know its id) -> clear map
            this._pending.clear();
          }
          break;
        }
      }
    } catch {
      // swallow errors to avoid throwing inside receiveMessage
    }
  }
}

function safeParseJSON(s: unknown) {
  try {
    if (typeof s === "string") {
      return JSON.parse(s);
    }
    return s;
  } catch {
    try {
      console.error(
        "[NRProfileManagerChild] safeParseJSON: failed to parse",
        s,
      );
    } catch {
      /* ignore logging failure */
    }
    return s;
  }
}
