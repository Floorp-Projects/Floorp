/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
