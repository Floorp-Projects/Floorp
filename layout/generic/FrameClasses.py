# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
# Frame class definitions, used to generate FrameIdList.h and FrameTypeList.h

from FrameClass import AbstractFrame, Frame

# Most frames support these.
COMMON = {
    "SupportsCSSTransforms",
    "SupportsContainLayoutAndPaint",
    "SupportsAspectRatio",
}
LEAF = {"Leaf"}
MATHML = {"MathML"}
SVG = {"SVG"}
BFC = {"BlockFormattingContext"}
LINE_PARTICIPANT = {"LineParticipant"}

BLOCK = COMMON | {"CanContainOverflowContainers"}

REPLACED = COMMON | {"Replaced"}
REPLACED_SIZING = REPLACED | {"ReplacedSizing"}

TABLE = COMMON - {"SupportsCSSTransforms"}
TABLE_PART = {"SupportsCSSTransforms", "TablePart"}
TABLE_CELL = TABLE_PART | {"SupportsContainLayoutAndPaint"}
MATHML_CONTAINER = (COMMON - {"SupportsContainLayoutAndPaint"}) | MATHML
SVG_CONTENT = (COMMON - {"SupportsContainLayoutAndPaint"}) | SVG
SVG_CONTAINER = SVG_CONTENT | {"SVGContainer"}

# NOTE: Intentionally not including "COMMON" here.
INLINE = LINE_PARTICIPANT | {"BidiInlineContainer"}
RUBY_CONTENT = LINE_PARTICIPANT
# FIXME(bug 713387): Shouldn't be Replaced, probably.
TEXT = COMMON | LINE_PARTICIPANT | {"Replaced"} | LEAF

# See FrameClass.py and GenerateFrameLists.py for implementation details.
# The following is a list of all the frame classes, followed by the frame type,
# and a set of flags.
#
# The frame type is somewhat arbitrary (could literally be anything) but for
# new frame class implementations it's probably a good idea to make it a unique
# string (maybe matching the frame name).
#
# See bug 1555477 for some related discussion about the whole Type() set-up.
FRAME_CLASSES = [
    Frame("BRFrame", "Br", REPLACED | LINE_PARTICIPANT | LEAF),
    Frame("nsBCTableCellFrame", "TableCell", TABLE_CELL),
    Frame("nsBackdropFrame", "Backdrop", COMMON | LEAF),
    Frame("nsBlockFrame", "Block", BLOCK),
    Frame("nsCanvasFrame", "Canvas", BLOCK),
    Frame("nsCheckboxRadioFrame", "CheckboxRadio", REPLACED | LEAF),
    Frame("nsColorControlFrame", "ColorControl", REPLACED | LEAF),
    Frame("nsColumnSetFrame", "ColumnSet", COMMON),
    Frame("ColumnSetWrapperFrame", "ColumnSetWrapper", BLOCK | BFC),
    Frame("nsComboboxControlFrame", "ComboboxControl", REPLACED | LEAF),
    Frame("ComboboxLabelFrame", "Block", BLOCK),
    Frame("nsContinuingTextFrame", "Text", TEXT),
    Frame("nsDateTimeControlFrame", "DateTimeControl", REPLACED),
    Frame("nsFieldSetFrame", "FieldSet", BLOCK),
    Frame("nsFileControlFrame", "FileControl", REPLACED | LEAF | BFC),
    Frame("FileControlLabelFrame", "Block", BLOCK | LEAF),
    Frame("nsFirstLetterFrame", "Letter", INLINE),
    Frame("nsFloatingFirstLetterFrame", "Letter", INLINE - LINE_PARTICIPANT),
    Frame("nsFirstLineFrame", "Line", INLINE),
    Frame("nsFlexContainerFrame", "FlexContainer", BLOCK),
    Frame("nsIFrame", "None", COMMON),
    Frame("nsGfxButtonControlFrame", "GfxButtonControl", REPLACED | LEAF),
    Frame("nsGridContainerFrame", "GridContainer", BLOCK),
    Frame("nsHTMLButtonControlFrame", "HTMLButtonControl", REPLACED),
    Frame("nsHTMLCanvasFrame", "HTMLCanvas", REPLACED_SIZING),
    Frame("nsHTMLFramesetBlankFrame", "None", COMMON | LEAF),
    Frame("nsHTMLFramesetBorderFrame", "None", COMMON | LEAF),
    Frame("nsHTMLFramesetFrame", "FrameSet", COMMON | LEAF),
    Frame("nsImageControlFrame", "ImageControl", REPLACED_SIZING | LEAF),
    Frame("nsImageFrame", "Image", REPLACED_SIZING | {"LeafDynamic"}),
    Frame("nsInlineFrame", "Inline", INLINE),
    Frame("nsListControlFrame", "ListControl", REPLACED),
    Frame("nsMathMLmathBlockFrame", "Block", BLOCK | MATHML | BFC),
    Frame("nsMathMLmathInlineFrame", "Inline", INLINE | MATHML),
    Frame("nsMathMLmencloseFrame", "None", MATHML_CONTAINER),
    Frame("nsMathMLmfracFrame", "None", MATHML_CONTAINER),
    Frame("nsMathMLmmultiscriptsFrame", "None", MATHML_CONTAINER),
    Frame("nsMathMLmoFrame", "None", MATHML_CONTAINER),
    Frame("nsMathMLmpaddedFrame", "None", MATHML_CONTAINER),
    Frame("nsMathMLmrootFrame", "None", MATHML_CONTAINER),
    Frame("nsMathMLmrowFrame", "None", MATHML_CONTAINER),
    Frame("nsMathMLmspaceFrame", "None", MATHML_CONTAINER | LEAF),
    Frame("nsMathMLmsqrtFrame", "None", MATHML_CONTAINER),
    Frame("nsMathMLmtableFrame", "Table", TABLE | MATHML),
    Frame("nsMathMLmtableWrapperFrame", "TableWrapper", BLOCK | MATHML),
    Frame("nsMathMLmtdFrame", "TableCell", TABLE_CELL | MATHML),
    Frame("nsMathMLmtdInnerFrame", "Block", BLOCK | MATHML),
    Frame("nsMathMLmtrFrame", "TableRow", TABLE_PART | MATHML),
    Frame("nsMathMLmunderoverFrame", "None", MATHML_CONTAINER),
    Frame("nsMathMLTokenFrame", "None", MATHML_CONTAINER),
    Frame("nsMenuPopupFrame", "MenuPopup", BLOCK),
    Frame("nsMeterFrame", "Meter", REPLACED | LEAF),
    Frame("nsNumberControlFrame", "TextInput", REPLACED | LEAF),
    Frame("nsPageBreakFrame", "PageBreak", COMMON | LEAF),
    Frame("nsPageContentFrame", "PageContent", BLOCK),
    Frame("nsPageFrame", "Page", COMMON),
    Frame("nsPlaceholderFrame", "Placeholder", COMMON | LEAF),
    Frame("nsProgressFrame", "Progress", REPLACED | LEAF),
    Frame("nsRangeFrame", "Range", REPLACED | LEAF),
    Frame("nsRubyBaseContainerFrame", "RubyBaseContainer", RUBY_CONTENT),
    Frame("nsRubyBaseFrame", "RubyBase", RUBY_CONTENT),
    Frame("nsRubyFrame", "Ruby", RUBY_CONTENT),
    Frame("nsRubyTextContainerFrame", "RubyTextContainer", {"None"}),
    Frame("nsRubyTextFrame", "RubyText", RUBY_CONTENT),
    Frame("ScrollContainerFrame", "ScrollContainer", COMMON),
    Frame("SimpleXULLeafFrame", "SimpleXULLeaf", COMMON | LEAF),
    Frame("nsScrollbarButtonFrame", "SimpleXULLeaf", COMMON | LEAF),
    Frame("nsScrollbarFrame", "Scrollbar", COMMON),
    Frame("nsSearchControlFrame", "SearchControl", LEAF),
    Frame("nsSelectsAreaFrame", "Block", BLOCK | BFC),
    Frame("nsPageSequenceFrame", "PageSequence", COMMON),
    Frame("nsSliderFrame", "Slider", COMMON),
    Frame("nsSplitterFrame", "SimpleXULLeaf", COMMON | LEAF),
    Frame("nsSubDocumentFrame", "SubDocument", REPLACED_SIZING | LEAF),
    Frame("PrintedSheetFrame", "PrintedSheet", COMMON),
    Frame("SVGAFrame", "SVGA", SVG_CONTAINER),
    Frame("SVGClipPathFrame", "SVGClipPath", SVG_CONTAINER),
    Frame("SVGContainerFrame", "None", SVG_CONTAINER),
    Frame("SVGFEContainerFrame", "SVGFEContainer", SVG_CONTENT),
    Frame("SVGFEImageFrame", "SVGFEImage", SVG_CONTENT | LEAF),
    Frame("SVGFELeafFrame", "SVGFELeaf", SVG_CONTENT | LEAF),
    Frame("SVGFEUnstyledLeafFrame", "SVGFEUnstyledLeaf", SVG_CONTENT | LEAF),
    Frame("SVGFilterFrame", "SVGFilter", SVG_CONTAINER),
    Frame("SVGForeignObjectFrame", "SVGForeignObject", SVG_CONTENT),
    Frame("SVGGeometryFrame", "SVGGeometry", SVG_CONTENT | LEAF),
    Frame("SVGGFrame", "SVGG", SVG_CONTAINER),
    Frame("SVGImageFrame", "SVGImage", SVG_CONTENT | LEAF),
    Frame("SVGInnerSVGFrame", "SVGInnerSVG", SVG_CONTAINER),
    Frame("SVGLinearGradientFrame", "SVGLinearGradient", SVG_CONTAINER),
    Frame("SVGMarkerFrame", "SVGMarker", SVG_CONTAINER),
    Frame("SVGMarkerAnonChildFrame", "SVGMarkerAnonChild", SVG_CONTAINER),
    Frame("SVGMaskFrame", "SVGMask", SVG_CONTAINER),
    Frame(
        "SVGOuterSVGFrame",
        "SVGOuterSVG",
        SVG_CONTAINER | {"Replaced", "ReplacedSizing", "SupportsContainLayoutAndPaint"},
    ),
    Frame("SVGOuterSVGAnonChildFrame", "SVGOuterSVGAnonChild", SVG_CONTAINER),
    Frame("SVGPatternFrame", "SVGPattern", SVG_CONTAINER),
    Frame("SVGRadialGradientFrame", "SVGRadialGradient", SVG_CONTAINER),
    Frame("SVGStopFrame", "SVGStop", SVG_CONTENT | LEAF),
    Frame("SVGSwitchFrame", "SVGSwitch", SVG_CONTAINER),
    Frame("SVGSymbolFrame", "SVGSymbol", SVG_CONTAINER),
    Frame("SVGTextFrame", "SVGText", SVG_CONTAINER),
    # Not a leaf, though it always has a ShadowRoot, so in practice light DOM
    # children never render.
    Frame("SVGUseFrame", "SVGUse", SVG_CONTAINER),
    Frame("MiddleCroppingLabelFrame", "MiddleCroppingLabel", BLOCK | LEAF),
    Frame("SVGViewFrame", "SVGView", SVG_CONTENT | LEAF),
    Frame("nsTableCellFrame", "TableCell", TABLE_CELL),
    Frame("nsTableColFrame", "TableCol", TABLE_PART),
    Frame("nsTableColGroupFrame", "TableColGroup", TABLE_PART),
    Frame("nsTableFrame", "Table", TABLE),
    Frame("nsTableWrapperFrame", "TableWrapper", BLOCK),
    Frame("nsTableRowFrame", "TableRow", TABLE_PART),
    Frame("nsTableRowGroupFrame", "TableRowGroup", TABLE_PART),
    Frame("nsTextControlFrame", "TextInput", REPLACED | LEAF),
    Frame("nsTextFrame", "Text", TEXT),
    Frame("nsTreeBodyFrame", "SimpleXULLeaf", COMMON | LEAF),
    Frame("nsVideoFrame", "HTMLVideo", REPLACED_SIZING),
    Frame("nsAudioFrame", "HTMLVideo", REPLACED_SIZING - {"SupportsAspectRatio"}),
    Frame("ViewportFrame", "Viewport", COMMON),
    Frame("WBRFrame", "Wbr", COMMON | LEAF),
    # Non-concrete classes (for FrameIID use)
    AbstractFrame("MiddleCroppingBlockFrame"),
    AbstractFrame("nsContainerFrame"),
    AbstractFrame("nsLeafFrame"),
    AbstractFrame("nsMathMLFrame"),
    AbstractFrame("nsMathMLContainerFrame"),
    AbstractFrame("nsRubyContentFrame"),
    AbstractFrame("nsSplittableFrame"),
    AbstractFrame("SVGDisplayContainerFrame"),
    AbstractFrame("SVGGradientFrame"),
    AbstractFrame("SVGPaintServerFrame"),
    # Interfaces (for FrameIID use)
    AbstractFrame("nsIAnonymousContentCreator"),
    AbstractFrame("nsIFormControlFrame"),
    AbstractFrame("nsIMathMLFrame"),
    AbstractFrame("nsIPercentBSizeObserver"),
    AbstractFrame("nsIPopupContainer"),
    AbstractFrame("nsIScrollbarMediator"),
    AbstractFrame("nsISelectControlFrame"),
    AbstractFrame("nsIStatefulFrame"),
    AbstractFrame("ISVGDisplayableFrame"),
    AbstractFrame("ISVGSVGFrame"),
    AbstractFrame("nsITableCellLayout"),
    AbstractFrame("nsITableLayout"),
    AbstractFrame("nsITextControlFrame"),
]
