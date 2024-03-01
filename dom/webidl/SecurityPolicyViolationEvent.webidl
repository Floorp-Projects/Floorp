/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/webappsec-csp/#violation-events
 */

enum SecurityPolicyViolationEventDisposition
{
  "enforce", "report"
};

[Exposed=Window]
interface SecurityPolicyViolationEvent : Event
{
    constructor(DOMString type,
                optional SecurityPolicyViolationEventInit eventInitDict = {});

    readonly attribute DOMString      documentURI;
    readonly attribute DOMString      referrer;
    readonly attribute DOMString      blockedURI;
    readonly attribute DOMString      violatedDirective; // historical alias of effectiveDirective
    readonly attribute DOMString      effectiveDirective;
    readonly attribute DOMString      originalPolicy;
    readonly attribute DOMString      sourceFile;
    readonly attribute DOMString      sample;
    readonly attribute SecurityPolicyViolationEventDisposition disposition;
    readonly attribute unsigned short statusCode;
    readonly attribute unsigned long  lineNumber;
    readonly attribute unsigned long  columnNumber;
};

[GenerateInitFromJSON, GenerateToJSON]
dictionary SecurityPolicyViolationEventInit : EventInit
{
    DOMString      documentURI = "";
    DOMString      referrer = "";
    DOMString      blockedURI = "";
    DOMString      violatedDirective = "";
    DOMString      effectiveDirective = "";
    DOMString      originalPolicy = "";
    DOMString      sourceFile = "";
    DOMString      sample = "";
    SecurityPolicyViolationEventDisposition disposition = "enforce";
    unsigned short statusCode = 0;
    unsigned long  lineNumber = 0;
    unsigned long  columnNumber = 0;
};
