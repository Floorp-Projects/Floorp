// SPDX-License-Identifier: MPL-2.0

export type ValidationNativeTab = {
  linkedBrowser: { contentTitle: string };
  parentNode: Node | null;
};

export type ValidationNativeWindow = {
  gBrowser: {
    addTab(
      url: string,
      options: { triggeringPrincipal: nsIPrincipal },
    ): ValidationNativeTab;
    removeTab(tab: ValidationNativeTab): void;
  };
};

export type ValidationExtensionPolicy = WebExtensionPolicy & {
  extension: { principal: nsIPrincipal };
};
