/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Patrick C. Beard <beard@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/**
 * Represents a simple object type, associating name with size.
 */
class Type {
	String mName;
	int mSize;

	Type(String name, int size) {
		mName = name;
		mSize = size;
	}

	public int hashCode() {
		return mName.hashCode() + mSize;
	}

	public boolean equals(Object obj) {
		if (obj instanceof Type) {
			Type t = (Type) obj;
			return (t.mSize == mSize && t.mName.equals(mName));
		}
		return false;
	}

	public String toString() {
		return "<A HREF=\"#" + mName + "_" + mSize + "\">&LT;" + mName + "&GT;</A> (" + mSize + ")";
	}

	static class Comparator extends QuickSort.Comparator {
		public int compare(Object obj1, Object obj2) {
			Type t1 = (Type) obj1, t2 = (Type) obj2;
			return (t1.mSize - t2.mSize);
		}
	}
}
