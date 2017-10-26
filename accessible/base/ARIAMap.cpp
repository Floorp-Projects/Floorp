/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ARIAMap.h"

#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "Role.h"
#include "States.h"

#include "nsAttrName.h"
#include "nsWhitespaceTokenizer.h"

#include "mozilla/BinarySearch.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::a11y;
using namespace mozilla::a11y::aria;

static const uint32_t kGenericAccType = 0;

/**
 *  This list of WAI-defined roles are currently hardcoded.
 *  Eventually we will most likely be loading an RDF resource that contains this information
 *  Using RDF will also allow for role extensibility. See bug 280138.
 *
 *  Definition of nsRoleMapEntry contains comments explaining this table.
 *
 *  When no Role enum mapping exists for an ARIA role, the role will be exposed
 *  via the object attribute "xml-roles".
 */

static const nsRoleMapEntry sWAIRoleMaps[] =
{
  { // alert
    &nsGkAtoms::alert,
    roles::ALERT,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eAlert,
    kNoReqStates
  },
  { // alertdialog
    &nsGkAtoms::alertdialog,
    roles::DIALOG,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // application
    &nsGkAtoms::application,
    roles::APPLICATION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // article
    &nsGkAtoms::article,
    roles::ARTICLE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates,
    eReadonlyUntilEditable
  },
  { // banner
    &nsGkAtoms::banner,
    roles::NOTHING,
    kUseNativeRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // button
    &nsGkAtoms::button,
    roles::PUSHBUTTON,
    kUseMapRole,
    eNoValue,
    ePressAction,
    eNoLiveAttr,
    eButton,
    kNoReqStates
    // eARIAPressed is auto applied on any button
  },
  { // cell
    &nsGkAtoms::cell,
    roles::CELL,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eTableCell,
    kNoReqStates
  },
  { // checkbox
    &nsGkAtoms::checkbox,
    roles::CHECKBUTTON,
    kUseMapRole,
    eNoValue,
    eCheckUncheckAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates,
    eARIACheckableMixed,
    eARIAReadonly
  },
  { // columnheader
    &nsGkAtoms::columnheader,
    roles::COLUMNHEADER,
    kUseMapRole,
    eNoValue,
    eSortAction,
    eNoLiveAttr,
    eTableCell,
    kNoReqStates,
    eARIASelectableIfDefined,
    eARIAReadonlyOrEditableIfDefined
  },
  { // combobox, which consists of text input and popup
    &nsGkAtoms::combobox,
    roles::EDITCOMBOBOX,
    kUseMapRole,
    eNoValue,
    eOpenCloseAction,
    eNoLiveAttr,
    eCombobox,
    states::COLLAPSED | states::HASPOPUP,
    eARIAAutoComplete,
    eARIAReadonly,
    eARIAOrientation
  },
  { // complementary
    &nsGkAtoms::complementary,
    roles::NOTHING,
    kUseNativeRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // contentinfo
    &nsGkAtoms::contentinfo,
    roles::NOTHING,
    kUseNativeRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // dialog
    &nsGkAtoms::dialog,
    roles::DIALOG,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // directory
    &nsGkAtoms::directory,
    roles::LIST,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eList,
    states::READONLY
  },
  { // doc-abstract
    &nsGkAtoms::docAbstract,
    roles::SECTION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // doc-acknowledgments
    &nsGkAtoms::docAcknowledgments,
    roles::LANDMARK,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // doc-afterword
    &nsGkAtoms::docAfterword,
    roles::LANDMARK,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // doc-appendix
    &nsGkAtoms::docAppendix,
    roles::LANDMARK,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // doc-backlink
    &nsGkAtoms::docBacklink,
    roles::LINK,
    kUseMapRole,
    eNoValue,
    eJumpAction,
    eNoLiveAttr,
    kGenericAccType,
    states::LINKED
  },
  { // doc-biblioentry
    &nsGkAtoms::docBiblioentry,
    roles::LISTITEM,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    states::READONLY
  },
  { // doc-bibliography
    &nsGkAtoms::docBibliography,
    roles::LANDMARK,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // doc-biblioref
    &nsGkAtoms::docBiblioref,
    roles::LINK,
    kUseMapRole,
    eNoValue,
    eJumpAction,
    eNoLiveAttr,
    kGenericAccType,
    states::LINKED
  },
  { // doc-chapter
    &nsGkAtoms::docChapter,
    roles::LANDMARK,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // doc-colophon
    &nsGkAtoms::docColophon,
    roles::SECTION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // doc-conclusion
    &nsGkAtoms::docConclusion,
    roles::LANDMARK,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // doc-cover
    &nsGkAtoms::docCover,
    roles::GRAPHIC,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // doc-credit
    &nsGkAtoms::docCredit,
    roles::SECTION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // doc-credits
    &nsGkAtoms::docCredits,
    roles::LANDMARK,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // doc-dedication
    &nsGkAtoms::docDedication,
    roles::SECTION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // doc-endnote
    &nsGkAtoms::docEndnote,
    roles::LISTITEM,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    states::READONLY
  },
  { // doc-endnotes
    &nsGkAtoms::docEndnotes,
    roles::LANDMARK,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // doc-epigraph
    &nsGkAtoms::docEpigraph,
    roles::SECTION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // doc-epilogue
    &nsGkAtoms::docEpilogue,
    roles::LANDMARK,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // doc-errata
    &nsGkAtoms::docErrata,
    roles::LANDMARK,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // doc-example
    &nsGkAtoms::docExample,
    roles::SECTION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // doc-footnote
    &nsGkAtoms::docFootnote,
    roles::FOOTNOTE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // doc-foreword
    &nsGkAtoms::docForeword,
    roles::LANDMARK,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // doc-glossary
    &nsGkAtoms::docGlossary,
    roles::LANDMARK,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // doc-glossref
    &nsGkAtoms::docGlossref,
    roles::LINK,
    kUseMapRole,
    eNoValue,
    eJumpAction,
    eNoLiveAttr,
    kGenericAccType,
    states::LINKED
  },
  { // doc-index
    &nsGkAtoms::docIndex,
    roles::NAVIGATION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // doc-introduction
    &nsGkAtoms::docIntroduction,
    roles::LANDMARK,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // doc-noteref
    &nsGkAtoms::docNoteref,
    roles::LINK,
    kUseMapRole,
    eNoValue,
    eJumpAction,
    eNoLiveAttr,
    kGenericAccType,
    states::LINKED
  },
  { // doc-notice
    &nsGkAtoms::docNotice,
    roles::NOTE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // doc-pagebreak
    &nsGkAtoms::docPagebreak,
    roles::SEPARATOR,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // doc-pagelist
    &nsGkAtoms::docPagelist,
    roles::NAVIGATION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // doc-part
    &nsGkAtoms::docPart,
    roles::LANDMARK,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // doc-preface
    &nsGkAtoms::docPreface,
    roles::LANDMARK,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // doc-prologue
    &nsGkAtoms::docPrologue,
    roles::LANDMARK,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // doc-pullquote
    &nsGkAtoms::docPullquote,
    roles::SECTION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // doc-qna
    &nsGkAtoms::docQna,
    roles::SECTION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // doc-subtitle
    &nsGkAtoms::docSubtitle,
    roles::HEADING,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // doc-tip
    &nsGkAtoms::docTip,
    roles::NOTE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // doc-toc
    &nsGkAtoms::docToc,
    roles::NAVIGATION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // document
    &nsGkAtoms::document,
    roles::DOCUMENT,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates,
    eReadonlyUntilEditable
  },
  { // feed
    &nsGkAtoms::feed,
    roles::GROUPING,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // figure
    &nsGkAtoms::figure,
    roles::FIGURE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // form
    &nsGkAtoms::form,
    roles::FORM,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // grid
    &nsGkAtoms::grid,
    roles::TABLE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eSelect | eTable,
    kNoReqStates,
    eARIAMultiSelectable,
    eARIAReadonlyOrEditable,
    eFocusableUntilDisabled
  },
  { // gridcell
    &nsGkAtoms::gridcell,
    roles::GRID_CELL,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eTableCell,
    kNoReqStates,
    eARIASelectable,
    eARIAReadonlyOrEditableIfDefined
  },
  { // group
    &nsGkAtoms::group,
    roles::GROUPING,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // heading
    &nsGkAtoms::heading,
    roles::HEADING,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // img
    &nsGkAtoms::img,
    roles::GRAPHIC,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // key
    &nsGkAtoms::key,
    roles::KEY,
    kUseMapRole,
    eNoValue,
    ePressAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates,
    eARIAPressed
  },
  { // link
    &nsGkAtoms::link,
    roles::LINK,
    kUseMapRole,
    eNoValue,
    eJumpAction,
    eNoLiveAttr,
    kGenericAccType,
    states::LINKED
  },
  { // list
    &nsGkAtoms::list,
    roles::LIST,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eList,
    states::READONLY
  },
  { // listbox
    &nsGkAtoms::listbox,
    roles::LISTBOX,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eListControl | eSelect,
    states::VERTICAL,
    eARIAMultiSelectable,
    eARIAReadonly,
    eFocusableUntilDisabled,
    eARIAOrientation
  },
  { // listitem
    &nsGkAtoms::listitem,
    roles::LISTITEM,
    kUseMapRole,
    eNoValue,
    eNoAction, // XXX: should depend on state, parent accessible
    eNoLiveAttr,
    kGenericAccType,
    states::READONLY
  },
  { // log
    &nsGkAtoms::log_,
    roles::NOTHING,
    kUseNativeRole,
    eNoValue,
    eNoAction,
    ePoliteLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // main
    &nsGkAtoms::main,
    roles::NOTHING,
    kUseNativeRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // marquee
    &nsGkAtoms::marquee,
    roles::ANIMATION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eOffLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // math
    &nsGkAtoms::math,
    roles::FLAT_EQUATION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // menu
    &nsGkAtoms::menu,
    roles::MENUPOPUP,
    kUseMapRole,
    eNoValue,
    eNoAction, // XXX: technically accessibles of menupopup role haven't
               // any action, but menu can be open or close.
    eNoLiveAttr,
    kGenericAccType,
    states::VERTICAL,
    eARIAOrientation
  },
  { // menubar
    &nsGkAtoms::menubar,
    roles::MENUBAR,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    states::HORIZONTAL,
    eARIAOrientation
  },
  { // menuitem
    &nsGkAtoms::menuitem,
    roles::MENUITEM,
    kUseMapRole,
    eNoValue,
    eClickAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // menuitemcheckbox
    &nsGkAtoms::menuitemcheckbox,
    roles::CHECK_MENU_ITEM,
    kUseMapRole,
    eNoValue,
    eClickAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates,
    eARIACheckableMixed,
    eARIAReadonly
  },
  { // menuitemradio
    &nsGkAtoms::menuitemradio,
    roles::RADIO_MENU_ITEM,
    kUseMapRole,
    eNoValue,
    eClickAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates,
    eARIACheckableBool,
    eARIAReadonly
  },
  { // navigation
    &nsGkAtoms::navigation,
    roles::NOTHING,
    kUseNativeRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // none
    &nsGkAtoms::none,
    roles::NOTHING,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // note
    &nsGkAtoms::note_,
    roles::NOTE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // option
    &nsGkAtoms::option,
    roles::OPTION,
    kUseMapRole,
    eNoValue,
    eSelectAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates,
    eARIASelectable,
    eARIACheckedMixed
  },
  { // presentation
    &nsGkAtoms::presentation,
    roles::NOTHING,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // progressbar
    &nsGkAtoms::progressbar,
    roles::PROGRESSBAR,
    kUseMapRole,
    eHasValueMinMax,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    states::READONLY,
    eIndeterminateIfNoValue
  },
  { // radio
    &nsGkAtoms::radio,
    roles::RADIOBUTTON,
    kUseMapRole,
    eNoValue,
    eSelectAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates,
    eARIACheckableBool
  },
  { // radiogroup
    &nsGkAtoms::radiogroup,
    roles::RADIO_GROUP,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates,
    eARIAOrientation,
    eARIAReadonly
  },
  { // region
    &nsGkAtoms::region,
    roles::REGION,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // row
    &nsGkAtoms::row,
    roles::ROW,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eTableRow,
    kNoReqStates,
    eARIASelectable
  },
  { // rowgroup
    &nsGkAtoms::rowgroup,
    roles::GROUPING,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // rowheader
    &nsGkAtoms::rowheader,
    roles::ROWHEADER,
    kUseMapRole,
    eNoValue,
    eSortAction,
    eNoLiveAttr,
    eTableCell,
    kNoReqStates,
    eARIASelectableIfDefined,
    eARIAReadonlyOrEditableIfDefined
  },
  { // scrollbar
    &nsGkAtoms::scrollbar,
    roles::SCROLLBAR,
    kUseMapRole,
    eHasValueMinMax,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    states::VERTICAL,
    eARIAOrientation,
    eARIAReadonly
  },
  { // search
    &nsGkAtoms::search,
    roles::NOTHING,
    kUseNativeRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eLandmark,
    kNoReqStates
  },
  { // searchbox
    &nsGkAtoms::searchbox,
    roles::ENTRY,
    kUseMapRole,
    eNoValue,
    eActivateAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates,
    eARIAAutoComplete,
    eARIAMultiline,
    eARIAReadonlyOrEditable
  },
  { // separator
    &nsGkAtoms::separator_,
    roles::SEPARATOR,
    kUseMapRole,
    eHasValueMinMaxIfFocusable,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    states::HORIZONTAL,
    eARIAOrientation
  },
  { // slider
    &nsGkAtoms::slider,
    roles::SLIDER,
    kUseMapRole,
    eHasValueMinMax,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    states::HORIZONTAL,
    eARIAOrientation,
    eARIAReadonly
  },
  { // spinbutton
    &nsGkAtoms::spinbutton,
    roles::SPINBUTTON,
    kUseMapRole,
    eHasValueMinMax,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates,
    eARIAReadonly
  },
  { // status
    &nsGkAtoms::status,
    roles::STATUSBAR,
    kUseMapRole,
    eNoValue,
    eNoAction,
    ePoliteLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // switch
    &nsGkAtoms::_switch,
    roles::SWITCH,
    kUseMapRole,
    eNoValue,
    eCheckUncheckAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates,
    eARIACheckableBool,
    eARIAReadonly
  },
  { // tab
    &nsGkAtoms::tab,
    roles::PAGETAB,
    kUseMapRole,
    eNoValue,
    eSwitchAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates,
    eARIASelectable
  },
  { // table
    &nsGkAtoms::table,
    roles::TABLE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eTable,
    kNoReqStates,
    eARIASelectable
  },
  { // tablist
    &nsGkAtoms::tablist,
    roles::PAGETABLIST,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eSelect,
    states::HORIZONTAL,
    eARIAOrientation
  },
  { // tabpanel
    &nsGkAtoms::tabpanel,
    roles::PROPERTYPAGE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // term
    &nsGkAtoms::term,
    roles::TERM,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    states::READONLY
  },
  { // textbox
    &nsGkAtoms::textbox,
    roles::ENTRY,
    kUseMapRole,
    eNoValue,
    eActivateAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates,
    eARIAAutoComplete,
    eARIAMultiline,
    eARIAReadonlyOrEditable
  },
  { // timer
    &nsGkAtoms::timer,
    roles::NOTHING,
    kUseNativeRole,
    eNoValue,
    eNoAction,
    eOffLiveAttr,
    kNoReqStates
  },
  { // toolbar
    &nsGkAtoms::toolbar,
    roles::TOOLBAR,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    states::HORIZONTAL,
    eARIAOrientation
  },
  { // tooltip
    &nsGkAtoms::tooltip,
    roles::TOOLTIP,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates
  },
  { // tree
    &nsGkAtoms::tree,
    roles::OUTLINE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eSelect,
    states::VERTICAL,
    eARIAReadonly,
    eARIAMultiSelectable,
    eFocusableUntilDisabled,
    eARIAOrientation
  },
  { // treegrid
    &nsGkAtoms::treegrid,
    roles::TREE_TABLE,
    kUseMapRole,
    eNoValue,
    eNoAction,
    eNoLiveAttr,
    eSelect | eTable,
    kNoReqStates,
    eARIAReadonlyOrEditable,
    eARIAMultiSelectable,
    eFocusableUntilDisabled,
    eARIAOrientation
  },
  { // treeitem
    &nsGkAtoms::treeitem,
    roles::OUTLINEITEM,
    kUseMapRole,
    eNoValue,
    eActivateAction, // XXX: should expose second 'expand/collapse' action based
                     // on states
    eNoLiveAttr,
    kGenericAccType,
    kNoReqStates,
    eARIASelectable,
    eARIACheckedMixed
  }
};

static const nsRoleMapEntry sLandmarkRoleMap = {
  &nsGkAtoms::_empty,
  roles::NOTHING,
  kUseNativeRole,
  eNoValue,
  eNoAction,
  eNoLiveAttr,
  kGenericAccType,
  kNoReqStates
};

nsRoleMapEntry aria::gEmptyRoleMap = {
  &nsGkAtoms::_empty,
  roles::NOTHING,
  kUseMapRole,
  eNoValue,
  eNoAction,
  eNoLiveAttr,
  kGenericAccType,
  kNoReqStates
};

/**
 * Universal (Global) states:
 * The following state rules are applied to any accessible element,
 * whether there is an ARIA role or not:
 */
static const EStateRule sWAIUnivStateMap[] = {
  eARIABusy,
  eARIACurrent,
  eARIADisabled,
  eARIAExpanded,  // Currently under spec review but precedent exists
  eARIAHasPopup,  // Note this is a tokenised attribute starting in ARIA 1.1
  eARIAInvalid,
  eARIAModal,
  eARIARequired,  // XXX not global, Bug 553117
  eARIANone
};


/**
 * ARIA attribute map for attribute characteristics.
 * @note ARIA attributes that don't have any flags are not included here.
 */

struct AttrCharacteristics
{
  nsStaticAtom** attributeName;
  const uint8_t characteristics;
};

static const AttrCharacteristics gWAIUnivAttrMap[] = {
  {&nsGkAtoms::aria_activedescendant,  ATTR_BYPASSOBJ                               },
  {&nsGkAtoms::aria_atomic,   ATTR_BYPASSOBJ_IF_FALSE | ATTR_VALTOKEN | ATTR_GLOBAL },
  {&nsGkAtoms::aria_busy,                               ATTR_VALTOKEN | ATTR_GLOBAL },
  {&nsGkAtoms::aria_checked,           ATTR_BYPASSOBJ | ATTR_VALTOKEN               }, /* exposes checkable obj attr */
  {&nsGkAtoms::aria_controls,          ATTR_BYPASSOBJ                 | ATTR_GLOBAL },
  {&nsGkAtoms::aria_describedby,       ATTR_BYPASSOBJ                 | ATTR_GLOBAL },
  {&nsGkAtoms::aria_details,           ATTR_BYPASSOBJ                 | ATTR_GLOBAL },
  {&nsGkAtoms::aria_disabled,          ATTR_BYPASSOBJ | ATTR_VALTOKEN | ATTR_GLOBAL },
  {&nsGkAtoms::aria_dropeffect,                         ATTR_VALTOKEN | ATTR_GLOBAL },
  {&nsGkAtoms::aria_errormessage,      ATTR_BYPASSOBJ                 | ATTR_GLOBAL },
  {&nsGkAtoms::aria_expanded,          ATTR_BYPASSOBJ | ATTR_VALTOKEN               },
  {&nsGkAtoms::aria_flowto,            ATTR_BYPASSOBJ                 | ATTR_GLOBAL },
  {&nsGkAtoms::aria_grabbed,                            ATTR_VALTOKEN | ATTR_GLOBAL },
  {&nsGkAtoms::aria_haspopup,          ATTR_BYPASSOBJ_IF_FALSE | ATTR_VALTOKEN | ATTR_GLOBAL },
  {&nsGkAtoms::aria_hidden,            ATTR_BYPASSOBJ | ATTR_VALTOKEN | ATTR_GLOBAL }, /* handled special way */
  {&nsGkAtoms::aria_invalid,           ATTR_BYPASSOBJ | ATTR_VALTOKEN | ATTR_GLOBAL },
  {&nsGkAtoms::aria_label,             ATTR_BYPASSOBJ                 | ATTR_GLOBAL },
  {&nsGkAtoms::aria_labelledby,        ATTR_BYPASSOBJ                 | ATTR_GLOBAL },
  {&nsGkAtoms::aria_level,             ATTR_BYPASSOBJ                               }, /* handled via groupPosition */
  {&nsGkAtoms::aria_live,                               ATTR_VALTOKEN | ATTR_GLOBAL },
  {&nsGkAtoms::aria_modal,             ATTR_BYPASSOBJ | ATTR_VALTOKEN | ATTR_GLOBAL },
  {&nsGkAtoms::aria_multiline,         ATTR_BYPASSOBJ | ATTR_VALTOKEN               },
  {&nsGkAtoms::aria_multiselectable,   ATTR_BYPASSOBJ | ATTR_VALTOKEN               },
  {&nsGkAtoms::aria_owns,              ATTR_BYPASSOBJ                 | ATTR_GLOBAL },
  {&nsGkAtoms::aria_orientation,                        ATTR_VALTOKEN               },
  {&nsGkAtoms::aria_posinset,          ATTR_BYPASSOBJ                               }, /* handled via groupPosition */
  {&nsGkAtoms::aria_pressed,           ATTR_BYPASSOBJ | ATTR_VALTOKEN               },
  {&nsGkAtoms::aria_readonly,          ATTR_BYPASSOBJ | ATTR_VALTOKEN               },
  {&nsGkAtoms::aria_relevant,          ATTR_BYPASSOBJ                 | ATTR_GLOBAL },
  {&nsGkAtoms::aria_required,          ATTR_BYPASSOBJ | ATTR_VALTOKEN               },
  {&nsGkAtoms::aria_selected,          ATTR_BYPASSOBJ | ATTR_VALTOKEN               },
  {&nsGkAtoms::aria_setsize,           ATTR_BYPASSOBJ                               }, /* handled via groupPosition */
  {&nsGkAtoms::aria_sort,                               ATTR_VALTOKEN               },
  {&nsGkAtoms::aria_valuenow,          ATTR_BYPASSOBJ                               },
  {&nsGkAtoms::aria_valuemin,          ATTR_BYPASSOBJ                               },
  {&nsGkAtoms::aria_valuemax,          ATTR_BYPASSOBJ                               },
  {&nsGkAtoms::aria_valuetext,         ATTR_BYPASSOBJ                               }
};

namespace {

struct RoleComparator
{
  const nsDependentSubstring& mRole;
  explicit RoleComparator(const nsDependentSubstring& aRole) : mRole(aRole) {}
  int operator()(const nsRoleMapEntry& aEntry) const {
    return Compare(mRole, aEntry.ARIARoleString());
  }
};

}

const nsRoleMapEntry*
aria::GetRoleMap(dom::Element* aEl)
{
  return GetRoleMapFromIndex(GetRoleMapIndex(aEl));
}

uint8_t
aria::GetRoleMapIndex(dom::Element* aEl)
{
  nsAutoString roles;
  if (!aEl || !aEl->GetAttr(kNameSpaceID_None, nsGkAtoms::role, roles) ||
      roles.IsEmpty()) {
    // We treat role="" as if the role attribute is absent (per aria spec:8.1.1)
    return NO_ROLE_MAP_ENTRY_INDEX;
  }

  nsWhitespaceTokenizer tokenizer(roles);
  while (tokenizer.hasMoreTokens()) {
    // Do a binary search through table for the next role in role list
    const nsDependentSubstring role = tokenizer.nextToken();
    size_t idx;
    if (BinarySearchIf(sWAIRoleMaps, 0, ArrayLength(sWAIRoleMaps),
                       RoleComparator(role), &idx)) {
      return idx;
    }
  }

  // Always use some entry index if there is a non-empty role string
  // To ensure an accessible object is created
  return LANDMARK_ROLE_MAP_ENTRY_INDEX;
}


const nsRoleMapEntry*
aria::GetRoleMapFromIndex(uint8_t aRoleMapIndex)
{
  switch (aRoleMapIndex) {
    case NO_ROLE_MAP_ENTRY_INDEX:
      return nullptr;
    case EMPTY_ROLE_MAP_ENTRY_INDEX:
      return &gEmptyRoleMap;
    case LANDMARK_ROLE_MAP_ENTRY_INDEX:
      return &sLandmarkRoleMap;
    default:
      return sWAIRoleMaps + aRoleMapIndex;
  }
}

uint8_t
aria::GetIndexFromRoleMap(const nsRoleMapEntry* aRoleMapEntry)
{
  if (aRoleMapEntry == nullptr) {
    return NO_ROLE_MAP_ENTRY_INDEX;
  } else if (aRoleMapEntry == &gEmptyRoleMap) {
    return EMPTY_ROLE_MAP_ENTRY_INDEX;
  } else if (aRoleMapEntry == &sLandmarkRoleMap) {
      return LANDMARK_ROLE_MAP_ENTRY_INDEX;
  } else {
    return aRoleMapEntry - sWAIRoleMaps;
  }
}

uint64_t
aria::UniversalStatesFor(mozilla::dom::Element* aElement)
{
  uint64_t state = 0;
  uint32_t index = 0;
  while (MapToState(sWAIUnivStateMap[index], aElement, &state))
    index++;

  return state;
}

uint8_t
aria::AttrCharacteristicsFor(nsAtom* aAtom)
{
  for (uint32_t i = 0; i < ArrayLength(gWAIUnivAttrMap); i++)
    if (*gWAIUnivAttrMap[i].attributeName == aAtom)
      return gWAIUnivAttrMap[i].characteristics;

  return 0;
}

bool
aria::HasDefinedARIAHidden(nsIContent* aContent)
{
  return aContent &&
    nsAccUtils::HasDefinedARIAToken(aContent, nsGkAtoms::aria_hidden) &&
    !aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::aria_hidden,
                           nsGkAtoms::_false, eCaseMatters);
}

////////////////////////////////////////////////////////////////////////////////
// AttrIterator class

bool
AttrIterator::Next(nsAString& aAttrName, nsAString& aAttrValue)
{
  while (mAttrIdx < mAttrCount) {
    const nsAttrName* attr = mContent->GetAttrNameAt(mAttrIdx);
    mAttrIdx++;
    if (attr->NamespaceEquals(kNameSpaceID_None)) {
      nsAtom* attrAtom = attr->Atom();
      nsDependentAtomString attrStr(attrAtom);
      if (!StringBeginsWith(attrStr, NS_LITERAL_STRING("aria-")))
        continue; // Not ARIA

      uint8_t attrFlags = aria::AttrCharacteristicsFor(attrAtom);
      if (attrFlags & ATTR_BYPASSOBJ)
        continue; // No need to handle exposing as obj attribute here

      if ((attrFlags & ATTR_VALTOKEN) &&
           !nsAccUtils::HasDefinedARIAToken(mContent, attrAtom))
        continue; // only expose token based attributes if they are defined

      if ((attrFlags & ATTR_BYPASSOBJ_IF_FALSE) &&
          mContent->AttrValueIs(kNameSpaceID_None, attrAtom,
                                nsGkAtoms::_false, eCaseMatters)) {
        continue; // only expose token based attribute if value is not 'false'.
      }

      nsAutoString value;
      if (mContent->GetAttr(kNameSpaceID_None, attrAtom, value)) {
        aAttrName.Assign(Substring(attrStr, 5));
        aAttrValue.Assign(value);
        return true;
      }
    }
  }

  return false;
}

