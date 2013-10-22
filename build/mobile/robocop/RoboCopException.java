/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

public class RoboCopException extends RuntimeException {
    
    public RoboCopException() {
        super();
    }
    
    public RoboCopException(String message) {
        super(message);
    }
    
    public RoboCopException(Throwable cause) {
        super(cause);
    }
    
    public RoboCopException(String message, Throwable cause) {
        super(message, cause);
    }
}
