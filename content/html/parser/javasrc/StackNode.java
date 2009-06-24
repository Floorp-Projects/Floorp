/*
 * Copyright (c) 2007 Henri Sivonen
 * Copyright (c) 2007-2009 Mozilla Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

package nu.validator.htmlparser.impl;

import nu.validator.htmlparser.annotation.Local;
import nu.validator.htmlparser.annotation.NsUri;

final class StackNode<T> {
    final int group;

    final @Local String name;

    final @Local String popName;

    final @NsUri String ns;

    final T node;

    final boolean scoping;

    final boolean special;

    final boolean fosterParenting;
    
    private int refcount = 1;

    /**
     * @param group
     *            TODO
     * @param name
     * @param node
     * @param scoping
     * @param special
     * @param popName
     *            TODO
     */
    StackNode(int group, final @NsUri String ns, final @Local String name, final T node,
            final boolean scoping, final boolean special,
            final boolean fosterParenting, final @Local String popName) {
        this.group = group;
        this.name = name;
        this.popName = popName;
        this.ns = ns;
        this.node = node;
        this.scoping = scoping;
        this.special = special;
        this.fosterParenting = fosterParenting;
        this.refcount = 1;
        Portability.retainLocal(name);
        Portability.retainLocal(popName);
        Portability.retainElement(node);
        // not retaining namespace for now        
    }

    /**
     * @param elementName
     *            TODO
     * @param node
     */
    StackNode(final @NsUri String ns, ElementName elementName, final T node) {
        this.group = elementName.group;
        this.name = elementName.name;
        this.popName = elementName.name;
        this.ns = ns;
        this.node = node;
        this.scoping = elementName.scoping;
        this.special = elementName.special;
        this.fosterParenting = elementName.fosterParenting;
        this.refcount = 1;
        Portability.retainLocal(name);
        Portability.retainLocal(popName);
        Portability.retainElement(node);
        // not retaining namespace for now        
    }

    StackNode(final @NsUri String ns, ElementName elementName, final T node, @Local String popName) {
        this.group = elementName.group;
        this.name = elementName.name;
        this.popName = popName;
        this.ns = ns;
        this.node = node;
        this.scoping = elementName.scoping;
        this.special = elementName.special;
        this.fosterParenting = elementName.fosterParenting;
        this.refcount = 1;
        Portability.retainLocal(name);
        Portability.retainLocal(popName);
        Portability.retainElement(node);
        // not retaining namespace for now        
    }

    StackNode(final @NsUri String ns, ElementName elementName, final T node, @Local String popName, boolean scoping) {
        this.group = elementName.group;
        this.name = elementName.name;
        this.popName = popName;
        this.ns = ns;
        this.node = node;
        this.scoping = scoping;
        this.special = false;
        this.fosterParenting = false;
        this.refcount = 1;
        Portability.retainLocal(name);
        Portability.retainLocal(popName);
        Portability.retainElement(node);
        // not retaining namespace for now        
    }
    
    @SuppressWarnings("unused") private void destructor() {
        Portability.releaseLocal(name);
        Portability.releaseLocal(popName);
        Portability.releaseElement(node);
        // not releasing namespace for now        
    }
    
    // [NOCPP[
    /**
     * @see java.lang.Object#toString()
     */
    @Override public @Local String toString() {
        return name;
    }
    // ]NOCPP]
    
    public void retain() {   
        refcount++;
    }
    
    public void release() {
        refcount--;
        if (refcount == 0) {
            Portability.delete(this);
        }
    }
}
