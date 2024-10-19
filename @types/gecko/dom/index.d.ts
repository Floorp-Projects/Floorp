/**
 * NOTE: Do not modify this file by hand.
 * Content was generated from source .webidl files.
 */
/**
 * This file from https://phabricator.services.mozilla.com/D209620,
 * the source of https://bugzilla.mozilla.org/show_bug.cgi?id=1895418
 * in 2024-05-28 21:30 KST
 */

/// <reference path="../lib.gecko.xpcom.d.ts" />
/// <reference path="../lib.gecko.services.d.ts" />
/// <reference path="../lib.gecko.dom.d.ts" />
/// <reference path="../lib.gecko.tweaks.d.ts" />

/* NORA START */
declare var window: Window;
declare var Components: nsIXPCComponents;
declare var Cu: nsIXPCComponents_Utils;
declare var Ci: nsIXPCComponents_Interfaces;
declare var Services: JSServices;
declare var Cc: nsIXPCComponents_Classes & {
  [key: string]: {
    getService: (
      t: typeof Ci[keyof typeof Ci],
    ) => unknown;
  };
};
interface CSSStyleDeclaration {
    display: string;
    flex: string;
    order: string;
    flexDirection: string;
    flexTemplateAreas: string;
}
/* NORA END */

// interface CSSStyleDeclaration {
//   display: string;
// }
