/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors.utils;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.util.Comparator;

public class AlphabeticAnnotatableEntityComparator<T extends Member> implements Comparator<T> {
    @Override
    public int compare(T aLhs, T aRhs) {
        // Constructors, Methods, Fields.
        boolean lIsConstructor = aLhs instanceof Constructor;
        boolean rIsConstructor = aRhs instanceof Constructor;
        boolean lIsMethod = aLhs instanceof Method;
        boolean rIsField = aRhs instanceof Field;

        if (lIsConstructor) {
            if (!rIsConstructor) {
                return -1;
            }
        } else if (lIsMethod) {
            if (rIsConstructor) {
                return 1;
            } else if (rIsField) {
                return -1;
            }
        } else {
            if (!rIsField) {
                return 1;
            }
        }

        // Verify these objects are the same type and cast them.
        if (aLhs instanceof Method) {
            return compare((Method) aLhs, (Method) aRhs);
        } else if (aLhs instanceof Field) {
            return compare((Field) aLhs, (Field) aRhs);
        } else {
            return compare((Constructor) aLhs, (Constructor) aRhs);
        }
    }

    // Alas, the type system fails us.
    private static int compare(Method aLhs, Method aRhs) {
        // Initially, attempt to differentiate the methods be name alone..
        String lName = aLhs.getName();
        String rName = aRhs.getName();

        int ret = lName.compareTo(rName);
        if (ret != 0) {
            return ret;
        }

        // The names were the same, so we need to compare signatures to find their uniqueness..
        lName = Utils.getTypeSignatureStringForMethod(aLhs);
        rName = Utils.getTypeSignatureStringForMethod(aRhs);

        return lName.compareTo(rName);
    }

    private static int compare(Constructor aLhs, Constructor aRhs) {
        // The names will be the same, so we need to compare signatures to find their uniqueness..
        String lName = Utils.getTypeSignatureString(aLhs);
        String rName = Utils.getTypeSignatureString(aRhs);

        return lName.compareTo(rName);
    }

    private static int compare(Field aLhs, Field aRhs) {
        // Compare field names..
        String lName = aLhs.getName();
        String rName = aRhs.getName();

        return lName.compareTo(rName);
    }
}
