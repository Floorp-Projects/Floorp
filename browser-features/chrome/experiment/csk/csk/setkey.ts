// SPDX-License-Identifier: MPL-2.0

import { effect, signal } from "@preact/signals";
import {
  type CSKCommands,
  type CSKData,
  zCSKData,
} from "@nora/shared/custom-shortcut-key/defines";
import type { commands } from "@nora/shared/custom-shortcut-key/commands";
import { checkIsSystemShortcut } from "@nora/shared/custom-shortcut-key/utils";

export const editingStatus = signal<string | null>(null);
export const setEditingStatus = (v: string | null): void => {
  editingStatus.value = v;
};

export const currentFocus = signal<keyof typeof commands | null>(null);
export const setCurrentFocus = (v: keyof typeof commands | null): void => {
  currentFocus.value = v;
};

effect(() => {
  // console.log(currentFocus.value !== null);
  Services.obs.notifyObservers(
    {},
    "nora-csk",
    JSON.stringify({
      type: "disable-csk",
      data: currentFocus.value !== null,
    } as CSKCommands),
  );
});

export const cskData = signal(
  //TODO: safely catch
  zCSKData.parse(
    JSON.parse(
      Services.prefs.getStringPref("floorp.browser.nora.csk.data", "{}"),
    ),
  ),
);

export function cskDatumToString(data: CSKData, key: keyof CSKData) {
  //TODO: Meta key
  if (key in data) {
    const datum = data[key];
    return `${datum?.modifiers.ctrl ? "Ctrl + " : ""}${
      datum?.modifiers.alt ? "Alt + " : ""
    }${datum?.modifiers.shift ? "Shift + " : ""}${datum?.key}`;
  }
  return "";
}

effect(() => {
  Services.prefs.setStringPref(
    "floorp.browser.nora.csk.data",
    JSON.stringify(cskData.value),
  );
  Services.obs.notifyObservers(
    {},
    "nora-csk",
    JSON.stringify({
      type: "update-pref",
    } as CSKCommands),
  );
});

export function initSetKey() {
  document.addEventListener("keydown", (ev) => {
    const key = ev.key;
    const alt = ev.altKey;
    const ctrl = ev.ctrlKey;
    const shift = ev.shiftKey;
    const meta = ev.metaKey;

    if (key === "Escape" || key === "Tab") {
      return;
    }

    const focus = currentFocus.value;

    if (!(alt || ctrl || shift || meta)) {
      if (key === "Delete" || key === "Backspace") {
        if (focus) {
          ev.preventDefault();
          const temp = cskData.value;
          for (const key of Object.keys(temp)) {
            if (key === focus) {
              delete temp[key];
              cskData.value = temp;
              setEditingStatus(cskDatumToString(cskData.value, focus));
              break;
            }
          }
          console.log(cskData.value);
        }
        return;
      }
    }

    // on register

    if (focus) {
      ev.preventDefault();
      if (
        ["Control", "Alt", "Meta", "Shift"].filter((k) => key.includes(k))
          .length === 0
      ) {
        if (checkIsSystemShortcut(ev)) {
          console.warn(`This Event is registered in System: ${ev}`);
          return;
        }
        cskData.value = {
          ...cskData.value,
          [focus]: {
            key: key,
            modifiers: {
              alt,
              ctrl,
              meta,
              shift,
            },
          },
        };
        setEditingStatus(cskDatumToString(cskData.value, focus));
      }
    }
  });
}
