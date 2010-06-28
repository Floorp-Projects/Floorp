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

#ifndef GFX_POINT_H
#define GFX_POINT_H

#include "nsMathUtils.h"

#include "gfxTypes.h"

/*
 * gfxSize and gfxIntSize -- please keep their member functions in sync.
 * also note: gfxIntSize may be replaced by nsIntSize at some point...
 */
struct THEBES_API gfxIntSize {
    PRInt32 width, height;

    gfxIntSize() {}
    gfxIntSize(PRInt32 _width, PRInt32 _height) : width(_width), height(_height) {}

    void SizeTo(PRInt32 _width, PRInt32 _height) {width = _width; height = _height;}

    int operator==(const gfxIntSize& s) const {
        return ((width == s.width) && (height == s.height));
    }
    int operator!=(const gfxIntSize& s) const {
        return ((width != s.width) || (height != s.height));
    }
    gfxIntSize operator+(const gfxIntSize& s) const {
        return gfxIntSize(width + s.width, height + s.height);
    }
    gfxIntSize operator-() const {
        return gfxIntSize(- width, - height);
    }
    gfxIntSize operator-(const gfxIntSize& s) const {
        return gfxIntSize(width - s.width, height - s.height);
    }
    gfxIntSize operator*(const PRInt32 v) const {
        return gfxIntSize(width * v, height * v);
    }
    gfxIntSize operator/(const PRInt32 v) const {
        return gfxIntSize(width / v, height / v);
    }
};

struct THEBES_API gfxSize {
    gfxFloat width, height;

    gfxSize() {}
    gfxSize(gfxFloat _width, gfxFloat _height) : width(_width), height(_height) {}
    gfxSize(const gfxIntSize& size) : width(size.width), height(size.height) {}

    void SizeTo(gfxFloat _width, gfxFloat _height) {width = _width; height = _height;}

    int operator==(const gfxSize& s) const {
        return ((width == s.width) && (height == s.height));
    }
    int operator!=(const gfxSize& s) const {
        return ((width != s.width) || (height != s.height));
    }
    gfxSize operator+(const gfxSize& s) const {
        return gfxSize(width + s.width, height + s.height);
    }
    gfxSize operator-() const {
        return gfxSize(- width, - height);
    }
    gfxSize operator-(const gfxSize& s) const {
        return gfxSize(width - s.width, height - s.height);
    }
    gfxSize operator*(const gfxFloat v) const {
        return gfxSize(width * v, height * v);
    }
    gfxSize operator/(const gfxFloat v) const {
        return gfxSize(width / v, height / v);
    }
};



struct THEBES_API gfxPoint {
    gfxFloat x, y;

    gfxPoint() { }
    gfxPoint(const gfxPoint& p) : x(p.x), y(p.y) {}
    gfxPoint(gfxFloat _x, gfxFloat _y) : x(_x), y(_y) {}

    void MoveTo(gfxFloat aX, gfxFloat aY) { x = aX; y = aY; }

    int operator==(const gfxPoint& p) const {
        return ((x == p.x) && (y == p.y));
    }
    int operator!=(const gfxPoint& p) const {
        return ((x != p.x) || (y != p.y));
    }
    const gfxPoint& operator+=(const gfxPoint& p) {
        x += p.x;
        y += p.y;
        return *this;
    }
    gfxPoint operator+(const gfxPoint& p) const {
        return gfxPoint(x + p.x, y + p.y);
    }
    gfxPoint operator+(const gfxSize& s) const {
        return gfxPoint(x + s.width, y + s.height);
    }
    gfxPoint operator-(const gfxPoint& p) const {
        return gfxPoint(x - p.x, y - p.y);
    }
    gfxPoint operator-(const gfxSize& s) const {
        return gfxPoint(x - s.width, y - s.height);
    }
    gfxPoint operator-() const {
        return gfxPoint(- x, - y);
    }
    gfxPoint operator*(const gfxFloat v) const {
        return gfxPoint(x * v, y * v);
    }
    gfxPoint operator/(const gfxFloat v) const {
        return gfxPoint(x / v, y / v);
    }
    // Round() is *not* rounding to nearest integer if the values are negative.
    // They are always rounding as floor(n + 0.5).
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=410748#c14
    // And if you need similar method which is using NS_round(), you should
    // create new |RoundAwayFromZero()| method.
    gfxPoint& Round() {
        x = NS_floor(x + 0.5);
        y = NS_floor(y + 0.5);
        return *this;
    }
};

#endif /* GFX_POINT_H */
