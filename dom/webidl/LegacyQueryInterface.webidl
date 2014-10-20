/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface nsISupports;
interface IID;

[NoInterfaceObject,
 // Need Exposed here, because this is a mixin onto things like Event
 // that are exposed in workers.
 Exposed=(Window,Worker,System)]
interface LegacyQueryInterface {
  // Legacy QueryInterface, only exposed to chrome or XBL code on the
  // main thread.
  [Exposed=Window]
  nsISupports queryInterface(IID iid);
};

Attr implements LegacyQueryInterface;
BarProp implements LegacyQueryInterface;
BoxObject implements LegacyQueryInterface;
CaretPosition implements LegacyQueryInterface;
Comment implements LegacyQueryInterface;
Crypto implements LegacyQueryInterface;
CSSPrimitiveValue implements LegacyQueryInterface;
CSSStyleDeclaration implements LegacyQueryInterface;
CSSValueList implements LegacyQueryInterface;
DOMImplementation implements LegacyQueryInterface;
DOMParser implements LegacyQueryInterface;
DOMStringMap implements LegacyQueryInterface;
DOMTokenList implements LegacyQueryInterface;
Document implements LegacyQueryInterface;
DocumentFragment implements LegacyQueryInterface;
DocumentType implements LegacyQueryInterface;
Element implements LegacyQueryInterface;
Event implements LegacyQueryInterface;
EventSource implements LegacyQueryInterface;
FileList implements LegacyQueryInterface;
FormData implements LegacyQueryInterface;
HTMLCollection implements LegacyQueryInterface;
History implements LegacyQueryInterface;
MimeTypeArray implements LegacyQueryInterface;
NamedNodeMap implements LegacyQueryInterface;
MutationObserver implements LegacyQueryInterface;
MutationRecord implements LegacyQueryInterface;
Navigator implements LegacyQueryInterface;
NodeIterator implements LegacyQueryInterface;
NodeList implements LegacyQueryInterface;
Notification implements LegacyQueryInterface;
OfflineResourceList implements LegacyQueryInterface;
PaintRequest implements LegacyQueryInterface;
PaintRequestList implements LegacyQueryInterface;
Performance implements LegacyQueryInterface;
Plugin implements LegacyQueryInterface;
PluginArray implements LegacyQueryInterface;
ProcessingInstruction implements LegacyQueryInterface;
Range implements LegacyQueryInterface;
Rect implements LegacyQueryInterface;
Selection implements LegacyQueryInterface;
SVGAnimatedEnumeration implements LegacyQueryInterface;
SVGAnimatedInteger implements LegacyQueryInterface;
SVGAnimatedNumber implements LegacyQueryInterface;
SVGAnimatedNumberList implements LegacyQueryInterface;
SVGAnimatedPreserveAspectRatio implements LegacyQueryInterface;
SVGAnimatedString implements LegacyQueryInterface;
SVGLengthList implements LegacyQueryInterface;
SVGNumberList implements LegacyQueryInterface;
SVGPathSegList implements LegacyQueryInterface;
SVGPoint implements LegacyQueryInterface;
SVGPointList implements LegacyQueryInterface;
SVGPreserveAspectRatio implements LegacyQueryInterface;
SVGRect implements LegacyQueryInterface;
SVGStringList implements LegacyQueryInterface;
SVGTransformList implements LegacyQueryInterface;
Screen implements LegacyQueryInterface;
StyleSheet implements LegacyQueryInterface;
Text implements LegacyQueryInterface;
Touch implements LegacyQueryInterface;
TouchList implements LegacyQueryInterface;
TreeColumns implements LegacyQueryInterface;
TreeWalker implements LegacyQueryInterface;
UndoManager implements LegacyQueryInterface;
ValidityState implements LegacyQueryInterface;
WebSocket implements LegacyQueryInterface;
Window implements LegacyQueryInterface;
XMLHttpRequest implements LegacyQueryInterface;
XMLHttpRequestUpload implements LegacyQueryInterface;
XMLSerializer implements LegacyQueryInterface;
XPathEvaluator implements LegacyQueryInterface;
