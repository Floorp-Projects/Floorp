/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * Jonas Sicking. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// XSLT elements
TX_ATOM(applyImports, "apply-imports");
TX_ATOM(applyTemplates, "apply-templates");
TX_ATOM(attribute, "attribute");
TX_ATOM(attributeSet, "attribute-set");
TX_ATOM(callTemplate, "call-template");
TX_ATOM(choose, "choose");
TX_ATOM(comment, "comment");
TX_ATOM(copy, "copy");
TX_ATOM(copyOf, "copy-of");
TX_ATOM(decimalFormat, "decimal-format");
TX_ATOM(element, "element");
TX_ATOM(forEach, "for-each");
TX_ATOM(_if, "if");
TX_ATOM(import, "import");
TX_ATOM(include, "include");
TX_ATOM(key, "key");
TX_ATOM(message, "message");
TX_ATOM(number, "number");
TX_ATOM(otherwise, "otherwise");
TX_ATOM(output, "output");
TX_ATOM(param, "param");
TX_ATOM(processingInstruction, "processing-instruction");
TX_ATOM(preserveSpace, "preserve-space");
TX_ATOM(sort, "sort");
TX_ATOM(stripSpace, "strip-space");
TX_ATOM(stylesheet, "stylesheet");
TX_ATOM(_template, "template");
TX_ATOM(text, "text");
TX_ATOM(transform, "transform");
TX_ATOM(valueOf, "value-of");
TX_ATOM(variable, "variable");
TX_ATOM(when, "when");
TX_ATOM(withParam, "with-param");

// XSLT attributes
TX_ATOM(case_order, "case-order");
TX_ATOM(cdataSectionElements, "cdata-section-elements");
TX_ATOM(count, "count");
TX_ATOM(dataType, "data-type");
TX_ATOM(decimalSeparator, "decimal-separator");
TX_ATOM(defaultSpace, "default-space");
TX_ATOM(digit, "digit");
TX_ATOM(doctypePublic, "doctype-public");
TX_ATOM(doctypeSystem, "doctype-system");
TX_ATOM(elements, "elements");
TX_ATOM(encoding, "encoding");
TX_ATOM(format, "format");
TX_ATOM(from, "from");
TX_ATOM(groupingSeparator, "grouping-separator");
TX_ATOM(href, "href");
TX_ATOM(indent, "indent");
TX_ATOM(infinity, "infinity");
TX_ATOM(lang, "lang");
TX_ATOM(level, "level");
TX_ATOM(match, "match");
TX_ATOM(method, "method");
TX_ATOM(mediaType, "media-type");
TX_ATOM(minusSign, "minus-sign");
TX_ATOM(mode, "mode");
TX_ATOM(name, "name");
TX_ATOM(_namespace, "namespace");
TX_ATOM(NaN, "NaN");
TX_ATOM(omitXmlDeclaration, "omit-xml-declaration");
TX_ATOM(order, "order");
TX_ATOM(patternSeparator, "pattern-separator");
TX_ATOM(perMille, "per-mille");
TX_ATOM(percent, "percent");
TX_ATOM(priority, "priority");
TX_ATOM(select, "select");
TX_ATOM(standalone, "standalone");
TX_ATOM(test, "test");
TX_ATOM(use, "use");
TX_ATOM(useAttributeSets, "use-attribute-sets");
TX_ATOM(value, "value");
TX_ATOM(version, "version");
TX_ATOM(zeroDigit, "zero-digit");

// XSLT functions
TX_ATOM(current, "current");
TX_ATOM(document, "document");
TX_ATOM(elementAvailable, "element-available");
TX_ATOM(formatNumber, "format-number");
TX_ATOM(functionAvailable, "function-available");
TX_ATOM(generateId, "generate-id");
TX_ATOM(unparsedEntityUri, "unparsed-entity-uri");
TX_ATOM(systemProperty, "system-property");
