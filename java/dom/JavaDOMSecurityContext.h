/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 */
#include "nsISecurityContext.h"

class JavaDOMSecurityContext : public nsISecurityContext {
public:
    ////////////////////////////////////////////////////////////////////////////
    // from nsISupports 
    
    NS_DECL_ISUPPORTS
    
    ////////////////////////////////////////////////////////////////////////////
    // from nsISecurityContext:
    
    /**
     * Get the security context to be used in LiveConnect.
     * This is used for JavaScript <--> Java.
     *
     * @param target        -- Possible target.
     * @param action        -- Possible action on the target.
     * @return              -- NS_OK if the target and action is permitted on the security context.
     *                      -- NS_FALSE otherwise.
     */
    NS_IMETHOD Implies(const char* target, const char* action, PRBool *bAllowedAccess);
    JavaDOMSecurityContext();
    virtual ~JavaDOMSecurityContext();
};

