/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors.utils;

import java.lang.reflect.Method;
import java.util.Comparator;

public class AlphabeticMethodComparator implements Comparator<Method> {
    @Override
    public int compare(Method lhs, Method rhs) {
        // Initially, attempt to differentiate the methods be name alone..
        String lName = lhs.getName();
        String rName = rhs.getName();

        int ret = lName.compareTo(rName);
        if (ret != 0) {
            return ret;
        }

        // The names were the same, so we need to compare signatures to find their uniqueness..
        lName = Utils.getTypeSignatureString(lhs);
        rName = Utils.getTypeSignatureString(rhs);

        return lName.compareTo(rName);
    }
}
