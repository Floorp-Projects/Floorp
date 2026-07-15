// SPDX-License-Identifier: MPL-2.0

import enUSBrowserChrome from "../../../../i18n/en-US/browser-chrome.json" with {
  type: "json",
};
import jaJPBrowserChrome from "../../../../i18n/ja-JP/browser-chrome.json" with {
  type: "json",
};

export type FloorpIPProtectionDisclosureStrings = Readonly<{
  title: string;
  toolbarActiveTooltip: string;
  toolbarInactiveTooltip: string;
  toolbarExcludedTooltip: string;
  toolbarPausedTooltip: string;
  toolbarErrorTooltip: string;
  helpTooltip: string;
  scope: string;
  fullDisclosure: string;
  unauthenticatedTitle: string;
  settingsPromoHeading: string;
  settingsPromoMessage: string;
  termsPrefix: string;
  termsOfUse: string;
  termsConjunction: string;
  privacyNotice: string;
  termsSuffix: string;
}>;

type FloorpIPProtectionDisclosureResource = Readonly<{
  title: string;
  toolbar: Readonly<{
    activeTooltip: string;
    inactiveTooltip: string;
    excludedTooltip: string;
    pausedTooltip: string;
    errorTooltip: string;
    helpTooltip: string;
  }>;
  scope: Readonly<{
    summary: string;
    fullDisclosure: string;
  }>;
  unauthenticated: Readonly<{
    title: string;
  }>;
  settings: Readonly<{
    promoHeading: string;
    promoMessage: string;
  }>;
  terms: Readonly<{
    prefix: string;
    termsOfUse: string;
    conjunction: string;
    privacyNotice: string;
    suffix: string;
  }>;
}>;

function createDisclosureStrings(
  resource: FloorpIPProtectionDisclosureResource,
): FloorpIPProtectionDisclosureStrings {
  return Object.freeze({
    title: resource.title,
    toolbarActiveTooltip: resource.toolbar.activeTooltip,
    toolbarInactiveTooltip: resource.toolbar.inactiveTooltip,
    toolbarExcludedTooltip: resource.toolbar.excludedTooltip,
    toolbarPausedTooltip: resource.toolbar.pausedTooltip,
    toolbarErrorTooltip: resource.toolbar.errorTooltip,
    helpTooltip: resource.toolbar.helpTooltip,
    scope: resource.scope.summary,
    fullDisclosure: resource.scope.fullDisclosure,
    unauthenticatedTitle: resource.unauthenticated.title,
    settingsPromoHeading: resource.settings.promoHeading,
    settingsPromoMessage: resource.settings.promoMessage,
    termsPrefix: resource.terms.prefix,
    termsOfUse: resource.terms.termsOfUse,
    termsConjunction: resource.terms.conjunction,
    privacyNotice: resource.terms.privacyNotice,
    termsSuffix: resource.terms.suffix,
  });
}

const ENGLISH_STRINGS = createDisclosureStrings(
  enUSBrowserChrome.ipProtection,
);
const JAPANESE_STRINGS = createDisclosureStrings(
  jaJPBrowserChrome.ipProtection,
);

export function resolveFloorpIPProtectionDisclosureStrings(
  locale: string,
): FloorpIPProtectionDisclosureStrings {
  return locale.toLowerCase() === "ja" || locale.toLowerCase().startsWith("ja-")
    ? JAPANESE_STRINGS
    : ENGLISH_STRINGS;
}

export function getFloorpIPProtectionDisclosureStrings(): FloorpIPProtectionDisclosureStrings {
  return resolveFloorpIPProtectionDisclosureStrings(
    Services.locale.appLocaleAsBCP47,
  );
}
