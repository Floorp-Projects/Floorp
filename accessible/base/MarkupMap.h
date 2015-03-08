/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

MARKUPMAP(a,
          New_HTMLLink,
          roles::LINK)

MARKUPMAP(abbr,
          New_HyperText,
          0)

MARKUPMAP(acronym,
          New_HyperText,
          0)

MARKUPMAP(article,
          New_HyperText,
          roles::DOCUMENT,
          Attr(xmlroles, article))

MARKUPMAP(aside,
          New_HyperText,
          roles::NOTE)

MARKUPMAP(blockquote,
          New_HyperText,
          roles::SECTION)

MARKUPMAP(dd,
          New_HTMLDefinition,
          roles::DEFINITION)

MARKUPMAP(div,
          nullptr,
          roles::SECTION)

MARKUPMAP(dl,
          New_HTMLList,
          roles::DEFINITION_LIST)

MARKUPMAP(dt,
          New_HTMLListitem,
          roles::TERM)

MARKUPMAP(figcaption,
          New_HTMLFigcaption,
          roles::CAPTION)

MARKUPMAP(figure,
          New_HTMLFigure,
          roles::FIGURE,
          Attr(xmlroles, figure))

MARKUPMAP(form,
          New_HyperText,
          roles::FORM)

MARKUPMAP(footer,
          New_HyperText,
          roles::FOOTER)

MARKUPMAP(header,
          New_HyperText,
          roles::HEADER)

MARKUPMAP(h1,
          New_HyperText,
          roles::HEADING)

MARKUPMAP(h2,
          New_HyperText,
          roles::HEADING)

MARKUPMAP(h3,
          New_HyperText,
          roles::HEADING)

MARKUPMAP(h4,
          New_HyperText,
          roles::HEADING)

MARKUPMAP(h5,
          New_HyperText,
          roles::HEADING)

MARKUPMAP(h6,
          New_HyperText,
          roles::HEADING)

MARKUPMAP(label,
          New_HTMLLabel,
          roles::LABEL)

MARKUPMAP(legend,
          New_HTMLLegend,
          roles::LABEL)

MARKUPMAP(li,
          New_HTMLListitem,
          0)

MARKUPMAP(nav,
          New_HyperText,
          roles::SECTION)

MARKUPMAP(ol,
          New_HTMLList,
          roles::LIST)

MARKUPMAP(option,
          New_HTMLOption,
          0)

MARKUPMAP(optgroup,
          New_HTMLOptgroup,
          0)

MARKUPMAP(output,
          New_HTMLOutput,
          roles::SECTION,
          Attr(live, polite))

MARKUPMAP(progress,
          New_HTMLProgress,
          0)

MARKUPMAP(q,
          New_HyperText,
          0)

MARKUPMAP(section,
          New_HyperText,
          roles::SECTION,
          Attr(xmlroles, region))

MARKUPMAP(time,
          New_HyperText,
          0,
          Attr(xmlroles, time),
          AttrFromDOM(datetime, datetime))

MARKUPMAP(td,
          New_HTMLTableHeaderCellIfScope,
          0)

MARKUPMAP(th,
          New_HTMLTableHeaderCell,
          0)

MARKUPMAP(ul,
          New_HTMLList,
          roles::LIST)
