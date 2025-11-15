//@ts-ignore: decorator
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRProfileManagerParent extends JSWindowActorParent {
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "NRProfileManager:GetFxAccountsInfo": {
        try {
          const FxAccounts = ChromeUtils.importESModule(
            "resource://gre/modules/FxAccounts.sys.mjs",
          ).getFxAccountsSingleton();

          const info = await FxAccounts.getSignedInUser();
          const id = message.data?.id;
          // stringify structured result to avoid privileged objects crossing into child/content
          const resultPayload =
            typeof info === "string" ? info : JSON.stringify(info);
          this.sendAsyncMessage("NRProfileManager:GetFxAccountsInfo", {
            id,
            result: resultPayload,
          });
        } catch (e) {
          const id = message.data?.id;
          this.sendAsyncMessage("NRProfileManager:GetFxAccountsInfo", {
            id,
            error: String(e),
          });
        }
        break;
      }

      case "NRProfileManager:OpenUrl": {
        try {
          const { url } = message.data || {};
          const win = Services.wm.getMostRecentWindow("navigator:browser");
          if (win) {
            // prefer opening in a new tab. Use any cast because this is chrome window API.
            try {
              // @ts-ignore: openUILinkIn exists on chrome browser windows
              (win as any).openUILinkIn(url, "tab");
            } catch {
              // fallback to creating a tab
              // @ts-ignore: gBrowser exists on chrome browser windows
              (win as any).gBrowser.addTab(url);
            }
          }
        } catch (e) {
          // ignore errors for open
        }
        break;
      }

      case "NRProfileManager:GetProfiles": {
        try {
          const profileService = (
            Components.classes["@mozilla.org/toolkit/profile-service;1"] as any
          ).getService(Components.interfaces.nsIToolkitProfileService) as any;

          const defaultProfile = (() => {
            try {
              return profileService.defaultProfile;
            } catch (_) {
              return null;
            }
          })();

          const currentProfile = profileService.currentProfile;

          const profiles: any[] = [];
          for (const profile of profileService.profiles) {
            let isCurrentProfile = profile == currentProfile;
            let isInUse = isCurrentProfile;
            if (!isInUse) {
              try {
                let lock = profile.lock({});
                lock.unlock();
              } catch (err) {
                const r = (err as any)?.result;
                if (
                  r != Cr.NS_ERROR_FILE_NOT_DIRECTORY &&
                  r != Cr.NS_ERROR_FILE_NOT_FOUND
                ) {
                  isInUse = true;
                }
              }
            }

            profiles.push({
              name: profile.name,
              rootDir: { path: profile.rootDir.path },
              localDir: { path: profile.localDir.path },
              isDefault: profile == defaultProfile,
              isCurrentProfile,
              isInUse,
            });
          }

          const id = message.data?.id;
          // stringify profiles to avoid privileged objects crossing to child/content
          const resultPayload = JSON.stringify(profiles);
          this.sendAsyncMessage("NRProfileManager:GetProfiles", {
            id,
            result: resultPayload,
          });
        } catch (e) {
          const id = message.data?.id;
          this.sendAsyncMessage("NRProfileManager:GetProfiles", {
            id,
            error: String(e),
          });
        }
        break;
      }

      case "NRProfileManager:OpenProfile": {
        try {
          const { profileIdentifier } = message.data || {};
          const profileService = (
            Components.classes["@mozilla.org/toolkit/profile-service;1"] as any
          ).getService(Components.interfaces.nsIToolkitProfileService) as any;

          let target = null;
          for (const p of profileService.profiles) {
            if (
              p.name === profileIdentifier ||
              p.rootDir?.path === profileIdentifier ||
              p.localDir?.path === profileIdentifier
            ) {
              target = p;
              break;
            }
          }
          if (target) {
            Services.startup.createInstanceWithProfile(target);
          }
        } catch {
          // ignore
        }
        break;
      }

      case "NRProfileManager:CreateProfileWizard": {
        try {
          const win = Services.wm.getMostRecentWindow("navigator:browser");
          if (win) {
            const profileService = (
              Components.classes[
                "@mozilla.org/toolkit/profile-service;1"
              ] as any
            ).getService(Components.interfaces.nsIToolkitProfileService) as any;

            const persistProfiles = () => {
              try {
                if (typeof profileService.asyncFlush === "function") {
                  const result = profileService.asyncFlush();
                  if (result && typeof result.catch === "function") {
                    result.catch((err: unknown) => {
                      console.error(
                        "[NRProfileManagerParent] asyncFlush failed after CreateProfile",
                        err,
                      );
                    });
                  }
                } else if (typeof profileService.flush === "function") {
                  profileService.flush();
                }
              } catch (err) {
                console.error(
                  "[NRProfileManagerParent] persistProfiles failed",
                  err,
                );
              }
            };

            const callbacks = {
              CreateProfile: (profile: any) => {
                if (!profile) {
                  return;
                }
                try {
                  profileService.defaultProfile = profile;
                } catch (err) {
                  console.error(
                    "[NRProfileManagerParent] setting default profile failed",
                    err,
                  );
                }
                persistProfiles();
              },
            };

            // @ts-ignore: openDialog is available on chrome windows
            (win as any).openDialog(
              "chrome://mozapps/content/profile/createProfileWizard.xhtml",
              "",
              "centerscreen,chrome,modal,titlebar",
              profileService,
              callbacks,
            );
          }
        } catch {
          // ignore
        }
        break;
      }

      case "NRProfileManager:FlushProfiles": {
        try {
          const profileService = (
            Components.classes["@mozilla.org/toolkit/profile-service;1"] as any
          ).getService(Components.interfaces.nsIToolkitProfileService) as any;
          profileService.flush();
        } catch {
          // ignore
        }
        break;
      }

      case "NRProfileManager:RemoveProfile": {
        try {
          const { profileIdentifier, deleteFiles } = message.data || {};
          const profileService = (
            Components.classes["@mozilla.org/toolkit/profile-service;1"] as any
          ).getService(Components.interfaces.nsIToolkitProfileService) as any;

          let target = null;
          for (const p of profileService.profiles) {
            if (
              p.name === profileIdentifier ||
              p.rootDir?.path === profileIdentifier ||
              p.localDir?.path === profileIdentifier
            ) {
              target = p;
              break;
            }
          }
          if (target) {
            try {
              target.removeInBackground(Boolean(deleteFiles));
              const id = message.data?.id;
              this.sendAsyncMessage("NRProfileManager:RemoveProfile", {
                id,
                result: true,
              });
            } catch (err) {
              const id = message.data?.id;
              this.sendAsyncMessage("NRProfileManager:RemoveProfile", {
                id,
                error: String(err),
              });
            }
          }
        } catch (e) {
          const id = message.data?.id;
          this.sendAsyncMessage("NRProfileManager:RemoveProfile", {
            id,
            error: String(e),
          });
        }
        break;
      }

      case "NRProfileManager:RenameProfile": {
        try {
          const { profileIdentifier, newName } = message.data || {};
          const profileService = (
            Components.classes["@mozilla.org/toolkit/profile-service;1"] as any
          ).getService(Components.interfaces.nsIToolkitProfileService) as any;

          let target = null;
          for (const p of profileService.profiles) {
            if (
              p.name === profileIdentifier ||
              p.rootDir?.path === profileIdentifier ||
              p.localDir?.path === profileIdentifier
            ) {
              target = p;
              break;
            }
          }
          if (target) {
            try {
              target.name = newName;
              const id = message.data?.id;
              this.sendAsyncMessage("NRProfileManager:RenameProfile", {
                id,
                result: true,
              });
            } catch (err) {
              const id = message.data?.id;
              this.sendAsyncMessage("NRProfileManager:RenameProfile", {
                id,
                error: String(err),
              });
            }
          }
        } catch (e) {
          const id = message.data?.id;
          this.sendAsyncMessage("NRProfileManager:RenameProfile", {
            id,
            error: String(e),
          });
        }
        break;
      }

      case "NRProfileManager:SetDefaultProfile": {
        try {
          const { profileIdentifier } = message.data || {};
          const profileService = (
            Components.classes["@mozilla.org/toolkit/profile-service;1"] as any
          ).getService(Components.interfaces.nsIToolkitProfileService) as any;

          let target = null;
          for (const p of profileService.profiles) {
            if (
              p.name === profileIdentifier ||
              p.rootDir?.path === profileIdentifier ||
              p.localDir?.path === profileIdentifier
            ) {
              target = p;
              break;
            }
          }
          if (target) {
            try {
              profileService.defaultProfile = target;
              const id = message.data?.id;
              this.sendAsyncMessage("NRProfileManager:SetDefaultProfile", {
                id,
                result: true,
              });
            } catch (err) {
              const id = message.data?.id;
              this.sendAsyncMessage("NRProfileManager:SetDefaultProfile", {
                id,
                error: String(err),
              });
            }
          }
        } catch (e) {
          const id = message.data?.id;
          this.sendAsyncMessage("NRProfileManager:SetDefaultProfile", {
            id,
            error: String(e),
          });
        }
        break;
      }

      case "NRProfileManager:Restart": {
        try {
          const { safeMode } = message.data || {};
          let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
            Ci.nsISupportsPRBool,
          );
          Services.obs.notifyObservers(
            cancelQuit,
            "quit-application-requested",
            "restart",
          );

          if (cancelQuit.data) {
            break;
          }

          const flags = Ci.nsIAppStartup
            ? Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
            : 0;
          if (safeMode) {
            Services.startup.restartInSafeMode(flags);
          } else {
            Services.startup.quit(flags);
          }
        } catch {
          // ignore
        }
        break;
      }

      case "NRProfileManager:GetCurrentProfile": {
        // const { SelectableProfileService } = ChromeUtils.importESModule(
        //   "resource:///modules/profiles/SelectableProfileService.sys.mjs",
        // );
        // const profile = SelectableProfileService.currentProfile;

        // Get profile directory
        const dirService = (
          Components.classes["@mozilla.org/file/directory_service;1"] as any
        ).getService(Components.interfaces.nsIProperties);

        const profileDir = dirService.get(
          "ProfD",
          Components.interfaces.nsIFile,
        );
        const profilePath = profileDir.path;
        const profileName = profileDir.leafName;

        // If caller provided an id, return structured data; otherwise keep old string behavior.
        const id = message.data?.id;
        const payload = { profileName, profilePath };
        if (id) {
          this.sendAsyncMessage("NRProfileManager:GetCurrentProfile", {
            id,
            result: JSON.stringify(payload),
          });
        } else {
          this.sendAsyncMessage(
            "NRProfileManager:GetCurrentProfile",
            JSON.stringify(payload),
          );
        }
        break;
      }
    }
  }
}
