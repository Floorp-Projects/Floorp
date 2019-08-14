#!/usr/bin/python
# vim: set shiftwidth=4 tabstop=8 autoindent expandtab:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# For general fontforge documentation, see:
#   http://fontforge.sourceforge.net/
# For fontforge scripting documentation, see:
#   http://fontforge.sourceforge.net/scripting-tutorial.html
#   http://fontforge.sourceforge.net/scripting.html
# and most importantly:
#   http://fontforge.sourceforge.net/python.html

# To install what you need, on Ubuntu,
#   sudo apt-get install python-fontforge

from __future__ import print_function
import fontforge

em = 1000

def newMathFont(aName):
    print("Generating %s.otf..." % aName, end="")
    mathFont = fontforge.font()
    mathFont.fontname = aName
    mathFont.familyname = aName
    mathFont.fullname = aName
    mathFont.copyright = "Copyright (c) 2014 Mozilla Corporation"
    mathFont.encoding = "UnicodeFull"

    # Create a space character. Also force the creation of some MATH subtables
    # so that OTS will not reject the MATH table.
    g = mathFont.createChar(ord(" "), "space")
    g.width = em
    g.italicCorrection = 0
    g.topaccent = 0
    g.mathKern.bottomLeft = tuple([(0,0)])
    g.mathKern.bottomRight = tuple([(0,0)])
    g.mathKern.topLeft = tuple([(0,0)])
    g.mathKern.topRight = tuple([(0,0)])
    mathFont[ord(" ")].horizontalVariants = "space"
    mathFont[ord(" ")].verticalVariants = "space"
    return mathFont

def saveMathFont(aFont):
    aFont.em = em
    aFont.ascent = aFont.descent = em/2
    aFont.hhea_ascent = aFont.os2_typoascent = aFont.os2_winascent = em/2
    aFont.descent = aFont.hhea_descent = em/2
    aFont.os2_typodescent = aFont.os2_windescent = em/2
    aFont.hhea_ascent_add = aFont.hhea_descent_add = 0
    aFont.os2_typoascent_add = aFont.os2_typodescent_add = 0
    aFont.os2_winascent_add = aFont.os2_windescent_add = 0
    aFont.os2_use_typo_metrics = True
    aFont.generate(aFont.fontname + ".otf")
    print(" done.")

def createSquareGlyph(aFont, aCodePoint):
    g = aFont.createChar(aCodePoint)
    p = g.glyphPen()
    p.moveTo(0, 0)
    p.lineTo(em, 0)
    p.lineTo(em, em)
    p.lineTo(0, em)
    p.closePath();

def createLLTriangleGlyph(aFont, aCodePoint):
    g = aFont.createChar(aCodePoint)
    p = g.glyphPen()
    p.moveTo(0, 0)
    p.lineTo(em, 0)
    p.lineTo(0, em)
    p.closePath();

def createURTriangleGlyph(aFont, aCodePoint):
    g = aFont.createChar(aCodePoint)
    p = g.glyphPen()
    p.moveTo(em, 0)
    p.lineTo(em, em)
    p.lineTo(0, em)
    p.closePath();

def createDiamondGlyph(aFont, aCodePoint):
    g = aFont.createChar(aCodePoint)
    p = g.glyphPen()
    p.moveTo(0, em/2)
    p.lineTo(em/2, 0)
    p.lineTo(em, em/2)
    p.lineTo(em/2, em)
    p.closePath();

################################################################################
# Glyph variants and constructions
f = newMathFont("stretchy")
nvariants = 3

# Draw boxes for the size variants and glues
for i in range(0, nvariants):
    s = em * (i + 1)

    g = f.createChar(-1, "h%d" % i)
    p = g.glyphPen()
    p.moveTo(0, -em)
    p.lineTo(0, em)
    p.lineTo(s, em)
    p.lineTo(s, -em)
    p.closePath()
    g.width = s

    g = f.createChar(-1, "v%d" % i)
    p = g.glyphPen()
    p.moveTo(0, 0)
    p.lineTo(0, s)
    p.lineTo(2 * em, s)
    p.lineTo(2 * em, 0)
    p.closePath();
    g.width = 2 * em

# Draw some pieces for stretchy operators
s = em * nvariants

g = f.createChar(-1, "left")
p = g.glyphPen()
p.moveTo(0, -2 * em)
p.lineTo(0, 2 * em)
p.lineTo(s, em)
p.lineTo(s, -em)
p.closePath();
g.width = s

g = f.createChar(-1, "right")
p = g.glyphPen()
p.moveTo(0, -em)
p.lineTo(0, em)
p.lineTo(s, 2 * em)
p.lineTo(s, -2 * em)
p.closePath();
g.width = s

g = f.createChar(-1, "hmid")
p = g.glyphPen()
p.moveTo(0, -em)
p.lineTo(0, em)
p.lineTo(s, 2 * em)
p.lineTo(2 * s, em)
p.lineTo(2 * s, -em)
p.lineTo(s, -2 * em)
p.closePath();
g.width = 2 * s

g = f.createChar(-1, "bottom")
p = g.glyphPen()
p.moveTo(0, 0)
p.lineTo(0, s)
p.lineTo(2 * em, s)
p.lineTo(4 * em, 0)
p.closePath();
g.width = 4 * em

g = f.createChar(-1, "top")
p = g.glyphPen()
p.moveTo(0, 0)
p.lineTo(4 * em, 0)
p.lineTo(2 * em, -s)
p.lineTo(0, -s)
p.closePath();
g.width = 4 * em

g = f.createChar(-1, "vmid")
p = g.glyphPen()
p.moveTo(0, s)
p.lineTo(2 * em, s)
p.lineTo(4 * em, 0)
p.lineTo(2 * em, -s)
p.lineTo(0, -s)
p.closePath();
g.width = 3 * em

# Create small rectangle of various size for some exotic arrows that are
# unlikely to be stretchable with standard fonts.
hstretchy = [
    0x219C, # leftwards wave arrow
    0x219D, # rightwards wave arrow
    0x219E, # leftwards two headed arrow
    0x21A0, # rightwards two headed arrow
    0x21A2  # leftwards arrow with tail
]
vstretchy = [
    0x219F, # upwards two headed arrow
    0x21A1, # downwards two headed arrow
    0x21A5, # upwards arrow from bar
    0x21A7, # downwards arrow from bar
    0x21A8  # up down arrow with base
]
for i in range(0, 1 + nvariants + 1):
    s = (i + 1) * em/10

    g = f.createChar(hstretchy[i])
    p = g.glyphPen()
    p.moveTo(0, -em/10)
    p.lineTo(0, em/10)
    p.lineTo(s, em/10)
    p.lineTo(s, -em/10)
    p.closePath()
    g.width = s

    g = f.createChar(vstretchy[i])
    p = g.glyphPen()
    p.moveTo(0, 0)
    p.lineTo(0, s)
    p.lineTo(2 * em/10, s)
    p.lineTo(2 * em/10, 0)
    p.closePath();
    g.width = 2 * em/10

# hstretchy[0] and vstretchy[0] have all the variants and the components. The others only have one of them.
s = em * nvariants

f[hstretchy[0]].horizontalVariants = "uni219C h0 h1 h2"
f[hstretchy[0]].horizontalComponents = (("left", False, 0, 0, s), \
("h2", True, 0, 0, s), ("hmid", False, 0, 0, 2 * s), ("h2", True, 0, 0, s), \
("right", False, 0, 0, s))

f[hstretchy[1]].horizontalVariants = "uni219D h0"
f[hstretchy[2]].horizontalVariants = "uni219E h1"
f[hstretchy[3]].horizontalVariants = "uni21A0 h2"
f[hstretchy[4]].horizontalVariants = "uni21A2 h2"
f[hstretchy[4]].horizontalComponents = f[hstretchy[0]].horizontalComponents

f[vstretchy[0]].verticalVariants = "uni219F v0 v1 v2"
f[vstretchy[0]].verticalComponents = (("bottom", False, 0, 0, s), \
("v2", True, 0, 0, s), ("vmid", False, 0, 0, 2 * s), ("v2", True, 0, 0, s), \
("top", False, 0, 0, s))

f[vstretchy[1]].verticalVariants = "uni21A1 v0"
f[vstretchy[2]].verticalVariants = "uni21A5 v1"
f[vstretchy[3]].verticalVariants = "uni21A7 v2"
f[vstretchy[4]].verticalVariants = "uni21A8"
f[vstretchy[4]].verticalComponents = f[vstretchy[0]].verticalComponents

################################################################################
# Testing DisplayOperatorMinHeight
f.math.DisplayOperatorMinHeight =  8 * em
largeop = [0x2A1B, 0x2A1C] # integral with overbar/underbar

# Draw boxes of size 1, 2, 7, 8, 9em.
for i in [1, 2, 7, 8, 9]:
    s = em * i
    if i == 1 or i == 2:
        g = f.createChar(largeop[i-1])
    else:
        g = f.createChar(-1, "L%d" % i)
    p = g.glyphPen()
    p.moveTo(0, 0)
    p.lineTo(0, s)
    p.lineTo(s, s)
    p.lineTo(s, 0)
    p.closePath();
    g.width = s

f[largeop[0]].verticalVariants = "uni2A1B L7 L8 L9"
f[largeop[1]].verticalVariants = "uni2A1C L8"

saveMathFont(f)

################################################################################
# Testing AxisHeight
f = newMathFont("axis-height-1")
f.math.AxisHeight = 0
createSquareGlyph(f, ord("+"))
saveMathFont(f)

f = newMathFont("axis-height-2")
f.math.AxisHeight = 20 * em
createSquareGlyph(f, ord("+"))
saveMathFont(f)

################################################################################
# Testing Fraction Parameters
f = newMathFont("fraction-1")
f.math.FractionRuleThickness = 20 * em
f.math.FractionNumeratorShiftUp = 0
f.math.FractionNumeratorDisplayStyleShiftUp = 0
f.math.FractionDenominatorShiftDown = 0
f.math.FractionNumeratorGapMin = 0
f.math.FractionDenominatorGapMin = 0
f.math.FractionNumeratorDisplayStyleShiftUp = 0
f.math.FractionDenominatorDisplayStyleShiftDown = 0
f.math.FractionNumeratorDisplayStyleGapMin = 0
f.math.FractionDenominatorDisplayStyleGapMin = 0
saveMathFont(f)

################################################################################
# Testing Limits Parameters
f = newMathFont("limits-5")
f.math.UpperLimitGapMin = 0
f.math.UpperLimitBaselineRiseMin = 0
f.math.LowerLimitGapMin = 0
f.math.LowerLimitBaselineDropMin = 0
f.math.AccentBaseHeight = 6 * em
f.math.FlattenedAccentBaseHeight = 3 * em
createSquareGlyph(f, ord("~"))
saveMathFont(f)

f = newMathFont("dtls-1")
createSquareGlyph(f, ord("a"))
createLLTriangleGlyph(f, ord("b"))
createSquareGlyph(f, ord("c"))
createDiamondGlyph(f, 0x1D51E) #mathvariant=fraktur a
createURTriangleGlyph(f, 0x1D51F) #mathvariant=fraktur b
createDiamondGlyph(f, 0x1D520) #mathvariant=fraktur c
f.addLookup("gsub", "gsub_single", (), (("dtls", (("latn", ("dflt")),)),))
f.addLookupSubtable("gsub", "gsub_n")
glyph = f["a"]
glyph.addPosSub("gsub_n", "b")
glyph2 = f[0x1D51F]
glyph2.glyphname="urtriangle"
glyph3 = f[0x1D51E]
glyph3.addPosSub("gsub_n", "urtriangle")
saveMathFont(f)
