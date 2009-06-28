/*
 * Copyright (c) 2009 Mozilla Foundation
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


public class StateSnapshot<T> {

    /**
     * @param stack
     * @param listOfActiveFormattingElements
     * @param formPointer
     */
    StateSnapshot(StackNode<T>[] stack,
            StackNode<T>[] listOfActiveFormattingElements, T formPointer) {
        this.stack = stack;
        this.listOfActiveFormattingElements = listOfActiveFormattingElements;
        this.formPointer = formPointer;
    }

    final StackNode<T>[] stack;

    final StackNode<T>[] listOfActiveFormattingElements;

    final T formPointer;
    
    @SuppressWarnings("unused") private void destructor() {
        for (int i = 0; i < stack.length; i++) {
            stack[i].release();
        }
        Portability.releaseArray(stack);
        for (int i = 0; i < listOfActiveFormattingElements.length; i++) {
            if (listOfActiveFormattingElements[i] != null) {
                listOfActiveFormattingElements[i].release();                
            }
        }
        Portability.releaseArray(listOfActiveFormattingElements);
        Portability.retainElement(formPointer);
    }
}
