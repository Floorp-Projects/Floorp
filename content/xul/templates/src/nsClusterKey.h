/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsClusterKey_h__
#define nsClusterKey_h__

#include "prtypes.h"
#include "nsISupports.h"
#include "nsRuleNetwork.h"
class nsTemplateRule;

/**
 * A match "cluster" is a group of matches that all share the same
 * values for their "container" (or parent) and "member" (or child)
 * variables.
 *
 * Only one match in a cluster can be "active": the active match is
 * the match that is used to generate content for the content model.
 * The matches in a cluster "compete" amongst each other;
 * specifically, the match that corresponds to the rule that is
 * declared first wins, and becomes active.
 *
 * The nsClusterKey is a hashtable key into the set of matches that are
 * currently competing: it consists of the container variable, its
 * value, the member variable, and its value.
 */
class nsClusterKey {
public:
    nsClusterKey() { MOZ_COUNT_CTOR(nsClusterKey); }

    /**
     * Construct a nsClusterKey from an instantiation and a rule. This
     * will use the rule to identify the container and member variables,
     * and then pull out their assignments from the instantiation.
     * @param aInstantiation the instantiation to use to determine
     *   variable values
     * @param aRule the rule to use to determine the member and container
     *   variables.
     */
    nsClusterKey(const Instantiation& aInstantiation, const nsTemplateRule* aRule);

    nsClusterKey(PRInt32 aContainerVariable,
               const Value& aContainerValue,
               PRInt32 aMemberVariable,
               const Value& aMemberValue)
        : mContainerVariable(aContainerVariable),
          mContainerValue(aContainerValue),
          mMemberVariable(aMemberVariable),
          mMemberValue(aMemberValue) {
        MOZ_COUNT_CTOR(nsClusterKey); }

    nsClusterKey(const nsClusterKey& aKey)
        : mContainerVariable(aKey.mContainerVariable),
          mContainerValue(aKey.mContainerValue),
          mMemberVariable(aKey.mMemberVariable),
          mMemberValue(aKey.mMemberValue) {
        MOZ_COUNT_CTOR(nsClusterKey); }

    ~nsClusterKey() { MOZ_COUNT_DTOR(nsClusterKey); }

    nsClusterKey& operator=(const nsClusterKey& aKey) {
        mContainerVariable = aKey.mContainerVariable;
        mContainerValue    = aKey.mContainerValue;
        mMemberVariable    = aKey.mMemberVariable;
        mMemberValue       = aKey.mMemberValue;
        return *this; }

    PRBool operator==(const nsClusterKey& aKey) const {
        return Equals(aKey); }

    PRBool operator!=(const nsClusterKey& aKey) const {
        return !Equals(aKey); }

    PRInt32     mContainerVariable;
    Value       mContainerValue;
    PRInt32     mMemberVariable;
    Value       mMemberValue;

    PLHashNumber Hash() const {
        PLHashNumber temp1;
        temp1 = mContainerValue.Hash();
        temp1 &= 0xffff;
        temp1 |= PLHashNumber(mContainerVariable) << 16;
        PLHashNumber temp2;
        temp2 = mMemberValue.Hash();
        temp2 &= 0xffff;
        temp2 |= PLHashNumber(mMemberVariable) << 16;
        return temp1 ^ temp2; }

    static PLHashNumber PR_CALLBACK HashClusterKey(const void* aKey);
    static PRIntn PR_CALLBACK CompareClusterKeys(const void* aLeft, const void* aRight);

protected:
    PRBool Equals(const nsClusterKey& aKey) const {
        return mContainerVariable == aKey.mContainerVariable &&
            mContainerValue == aKey.mContainerValue &&
            mMemberVariable == aKey.mMemberVariable &&
            mMemberValue == aKey.mMemberValue; }
};

#endif // nsClusterKey_h__
