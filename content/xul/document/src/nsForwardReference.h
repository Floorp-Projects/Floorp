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

#ifndef nsForwardReference_h__
#define nsForwardReference_h__

class nsForwardReference
{
protected:
    nsForwardReference() {}

public:
    virtual ~nsForwardReference() {}

    /**
     * Priority codes returned from GetPhase()
     */
    enum Phase {
        /** A dummy marker, used to indicate unstarted resolution */
        eStart,

        /** The initial pass, after which the content model will be
            fully built */
        eConstruction,

        /** A second pass, after which all 'magic attribute' hookup
            will have been performed */
        eHookup,

        /** A dummy marker, used in kPasses to indicate termination */
        eDone
    };

    /**
     * Forward references are categorized by 'priority', and all
     * forward references in a higher priority are resolved before any
     * reference in a lower priority. This variable specifies this
     * ordering. The last Priority is guaranteed to be eDone.
     */
    static const Phase kPasses[];

    /**
     * Get the state in which the forward reference should be resolved.
     * 'eConstruction' references are all resolved before 'eHookup' references
     * are resolved.
     *
     * @return the Phase in which the reference needs to be resolved
     */
    virtual Phase GetPhase() = 0;

    /**
     * Result codes returned from Resolve()
     */
    enum Result {
        /** Resolution succeeded, I'm done. */
        eResolve_Succeeded,

        /** Couldn't resolve, but try me later. */
        eResolve_Later,

        /** Something bad happened, don't try again. */
        eResolve_Error
    };

    /**
     * Attempt to resolve the forward reference.
     *
     * @return a Result that tells the resolver how to treat
     * the reference.
     */
    virtual Result Resolve() = 0;
};

#endif // nsForwardReference_h__
