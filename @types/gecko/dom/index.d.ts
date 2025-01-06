/**
 * NOTE: Do not modify this file by hand.
 * Content was generated from source .webidl files.
 */
/**
 * This file from https://phabricator.services.mozilla.com/D209620,
 * the source of https://bugzilla.mozilla.org/show_bug.cgi?id=1895418
 * Currently https://searchfox.org/mozilla-central/source/tools/%40types
 * in 2024-12-28 21:05 KST
 */

/// <reference types="../lib.gecko.xpcom.d.ts" />
/// <reference types="../lib.gecko.services.d.ts" />
/// <reference types="../lib.gecko.dom.d.ts" />
/// <reference types="../lib.gecko.tweaks.d.ts" />
/// <reference types="../lib.gecko.nsresult.d.ts" />

/* NORA START */
declare var window: Window;
declare var Components: nsIXPCComponents;
declare var Cu: nsIXPCComponents_Utils;
declare var Ci: nsIXPCComponents_Interfaces;
declare var Services: JSServices;
declare var Cc: nsIXPCComponents_Classes & {
  [key: string]: {
    getService: (t: (typeof Ci)[keyof typeof Ci]) => any;
    createInstance: (aClass: (typeof Ci)[keyof typeof Ci]) => any;
  };
};
interface CSSStyleDeclaration {
  display: string;
  flex: string;
  order: string;
  flexDirection: string;
  flexTemplateAreas: string;
}

interface nsIXPCComponents extends nsISupports {
  readonly interfaces: nsIXPCComponents_Interfaces;
  readonly results: nsIXPCComponents_Results;
  isSuccessCode(result: any): boolean;
  readonly classes: nsIXPCComponents_Classes;
  readonly stack: nsIStackFrame;
  readonly manager: nsIComponentManager;
  readonly utils: nsIXPCComponents_Utils;
  readonly ID: nsIXPCComponents_ID;
  readonly Exception: any;
  readonly Constructor: (aClass: any, aIID: any, aFlags: any) => any;
  returnCode: any;
}

/* NORA END */

// interface CSSStyleDeclaration {
//   display: string;
// }
