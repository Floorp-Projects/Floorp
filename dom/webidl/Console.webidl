/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information on this interface, please see
 * https://console.spec.whatwg.org/#console-namespace
 */

[Exposed=(Window,Worker,WorkerDebugger,Worklet),
 ClassString="Console",
 ProtoObjectHack]
namespace console {

  // NOTE: if you touch this namespace, remember to update the ConsoleInstance
  // interface as well! - dom/chrome-webidl/ConsoleInstance.webidl

  // Logging
  [UseCounter]
  undefined assert(optional boolean condition = false, any... data);
  [UseCounter]
  undefined clear();
  [UseCounter]
  undefined count(optional DOMString label = "default");
  [UseCounter]
  undefined countReset(optional DOMString label = "default");
  [UseCounter]
  undefined debug(any... data);
  [UseCounter]
  undefined error(any... data);
  [UseCounter]
  undefined info(any... data);
  [UseCounter]
  undefined log(any... data);
  [UseCounter]
  undefined table(any... data); // FIXME: The spec is still unclear about this.
  [UseCounter]
  undefined trace(any... data);
  [UseCounter]
  undefined warn(any... data);
  [UseCounter]
  undefined dir(any... data); // FIXME: This doesn't follow the spec yet.
  [UseCounter]
  undefined dirxml(any... data);

  // Grouping
  [UseCounter]
  undefined group(any... data);
  [UseCounter]
  undefined groupCollapsed(any... data);
  [UseCounter]
  undefined groupEnd();

  // Timing
  [UseCounter]
  undefined time(optional DOMString label = "default");
  [UseCounter]
  undefined timeLog(optional DOMString label = "default", any... data);
  [UseCounter]
  undefined timeEnd(optional DOMString label = "default");

  // Mozilla only or Webcompat methods

  [UseCounter]
  undefined _exception(any... data);
  [UseCounter]
  undefined timeStamp(optional any data);

  [UseCounter]
  undefined profile(any... data);
  [UseCounter]
  undefined profileEnd(any... data);

  [ChromeOnly]
  const boolean IS_NATIVE_CONSOLE = true;

  [ChromeOnly, NewObject]
  ConsoleInstance createInstance(optional ConsoleInstanceOptions options = {});
};
