/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s):
 * Norris Boyd
 * Roger Lawrence
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */


package org.mozilla.javascript.optimizer;

import org.mozilla.javascript.*;

import java.util.Vector;
import java.util.Hashtable;
import java.util.Enumeration;

public class FatBlock {

    public FatBlock(int startNodeIndex, int endNodeIndex, Node[] statementNodes)
    {
        itsShadowOfFormerSelf = new Block(startNodeIndex, endNodeIndex, statementNodes);
    }
    
    public Node getEndNode()
        { return itsShadowOfFormerSelf.getEndNode(); }
    
    public Block getSlimmerSelf()   
        { return itsShadowOfFormerSelf; }

    private Block[] reduceToArray(Hashtable h)
    {
        Block[] result = null;
        if (!h.isEmpty()) {
            result = new Block[h.size()];
            Enumeration enum = h.elements();
            int i = 0;
            while (enum.hasMoreElements()) {
                FatBlock fb = (FatBlock)(enum.nextElement());
                result[i++] = fb.itsShadowOfFormerSelf;
            }
        }
        return result;
    }

    Block diet()
    {
        itsShadowOfFormerSelf.setSuccessorList(reduceToArray(itsSuccessors));
        itsShadowOfFormerSelf.setPredecessorList(reduceToArray(itsPredecessors));
        return itsShadowOfFormerSelf;
    }
    
    public void addSuccessor(FatBlock b)  { itsSuccessors.put(b, b); }
    public void addPredecessor(FatBlock b)  { itsPredecessors.put(b, b); }
        
        // all the Blocks that come immediately after this
    private Hashtable itsSuccessors = new Hashtable(4);   
        // all the Blocks that come immediately before this
    private Hashtable itsPredecessors = new Hashtable(4);
    
    private Block itsShadowOfFormerSelf;
    
    
}
    
