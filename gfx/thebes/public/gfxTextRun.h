/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
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

class gfxTextRun {
    // these do not copy the text
    gfxTextRun(const char* ASCII, int length);
    gfxTextRun(const PRUnichar* unicode, int length);
 
    void SetFont(nsFont* font, nsIAtom* language);

    enum { ClusterStart = 0x1 } CharFlags;
    // ClusterStart: character is the first character of a cluster/glyph
    void GetCharacterFlags(int pos, int len, CharFlags* flags);

    struct Dimensions {
        double ascent, descent, width, leftBearing, rightBearing;
    };
    Dimensions MeasureText(int pos, int len);

    // Compute how many characters from this string starting at
    // character 'pos' and up to length 'len' fit
    // into the given width. 'breakflags' indicates our
    // preferences about where we allow breaks.
    // We will usually want to call MeasureText right afterwards,
    // the implementor could optimize for that.
    int GetCharsFit(int pos, int len, double width, int breakflags);

    int GetPositionInString(gfxPoint pt);
};
