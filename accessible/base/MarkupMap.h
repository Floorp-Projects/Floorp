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

MARKUPMAP(details,
          New_HyperText,
          roles::DETAILS)

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

MARKUPMAP(map,
          nullptr,
          roles::TEXT_CONTAINER)

MARKUPMAP(math,
          New_HyperText,
          roles::MATHML_MATH)

MARKUPMAP(mi_,
          New_HyperText,
          roles::MATHML_IDENTIFIER)

MARKUPMAP(mn_,
          New_HyperText,
          roles::MATHML_NUMBER)

MARKUPMAP(mo_,
          New_HyperText,
          roles::MATHML_OPERATOR,
          AttrFromDOM(accent_, accent_),
          AttrFromDOM(fence_, fence_),
          AttrFromDOM(separator_, separator_),
          AttrFromDOM(largeop_, largeop_))

MARKUPMAP(mtext_,
          New_HyperText,
          roles::MATHML_TEXT)

MARKUPMAP(ms_,
          New_HyperText,
          roles::MATHML_STRING_LITERAL)

MARKUPMAP(mglyph_,
          New_HyperText,
          roles::MATHML_GLYPH)

MARKUPMAP(mrow_,
          New_HyperText,
          roles::MATHML_ROW)

MARKUPMAP(mfrac_,
          New_HyperText,
          roles::MATHML_FRACTION,
          AttrFromDOM(bevelled_, bevelled_),
          AttrFromDOM(linethickness_, linethickness_))

MARKUPMAP(msqrt_,
          New_HyperText,
          roles::MATHML_SQUARE_ROOT)

MARKUPMAP(mroot_,
          New_HyperText,
          roles::MATHML_ROOT)

MARKUPMAP(mfenced_,
          New_HyperText,
          roles::MATHML_FENCED,
          AttrFromDOM(close, close),
          AttrFromDOM(open, open),
          AttrFromDOM(separators_, separators_))

MARKUPMAP(menclose_,
          New_HyperText,
          roles::MATHML_ENCLOSED,
          AttrFromDOM(notation_, notation_))

MARKUPMAP(mstyle_,
          New_HyperText,
          roles::MATHML_STYLE)

MARKUPMAP(msub_,
          New_HyperText,
          roles::MATHML_SUB)

MARKUPMAP(msup_,
          New_HyperText,
          roles::MATHML_SUP)

MARKUPMAP(msubsup_,
          New_HyperText,
          roles::MATHML_SUB_SUP)

MARKUPMAP(munder_,
          New_HyperText,
          roles::MATHML_UNDER,
          AttrFromDOM(accentunder_, accentunder_),
          AttrFromDOM(align, align))

MARKUPMAP(mover_,
          New_HyperText,
          roles::MATHML_OVER,
          AttrFromDOM(accent_, accent_),
          AttrFromDOM(align, align))

MARKUPMAP(munderover_,
          New_HyperText,
          roles::MATHML_UNDER_OVER,
          AttrFromDOM(accent_, accent_),
          AttrFromDOM(accentunder_, accentunder_),
          AttrFromDOM(align, align))

MARKUPMAP(mmultiscripts_,
          New_HyperText,
          roles::MATHML_MULTISCRIPTS)

MARKUPMAP(mtable_,
          New_HTMLTableAccessible,
          roles::MATHML_TABLE,
          AttrFromDOM(align, align),
          AttrFromDOM(columnlines_, columnlines_),
          AttrFromDOM(rowlines_, rowlines_))

MARKUPMAP(mlabeledtr_,
          New_HTMLTableRowAccessible,
          roles::MATHML_LABELED_ROW)

MARKUPMAP(mtr_,
          New_HTMLTableRowAccessible,
          roles::MATHML_TABLE_ROW)

MARKUPMAP(mtd_,
          New_HTMLTableCellAccessible,
          roles::MATHML_CELL)

MARKUPMAP(maction_,
          New_HyperText,
          roles::MATHML_ACTION,
          AttrFromDOM(actiontype_, actiontype_),
          AttrFromDOM(selection_, selection_))

MARKUPMAP(merror_,
          New_HyperText,
          roles::MATHML_ERROR)

MARKUPMAP(mstack_,
          New_HyperText,
          roles::MATHML_STACK,
          AttrFromDOM(align, align),
          AttrFromDOM(position, position))

MARKUPMAP(mlongdiv_,
          New_HyperText,
          roles::MATHML_LONG_DIVISION,
          AttrFromDOM(longdivstyle_, longdivstyle_))

MARKUPMAP(msgroup_,
          New_HyperText,
          roles::MATHML_STACK_GROUP,
          AttrFromDOM(position, position),
          AttrFromDOM(shift_, shift_))

MARKUPMAP(msrow_,
          New_HyperText,
          roles::MATHML_STACK_ROW,
          AttrFromDOM(position, position))

MARKUPMAP(mscarries_,
          New_HyperText,
          roles::MATHML_STACK_CARRIES,
          AttrFromDOM(location_, location_),
          AttrFromDOM(position, position))

MARKUPMAP(mscarry_,
          New_HyperText,
          roles::MATHML_STACK_CARRY,
          AttrFromDOM(crossout_, crossout_))

MARKUPMAP(msline_,
          New_HyperText,
          roles::MATHML_STACK_LINE,
          AttrFromDOM(position, position))

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

MARKUPMAP(p,
          nullptr,
          roles::PARAGRAPH)

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

MARKUPMAP(summary,
          New_HTMLSummary,
          roles::SUMMARY)

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
