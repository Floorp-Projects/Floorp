/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 *  except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/

 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is MozillaTranslator (Mozilla Localization Tool)
 *
 * The Initial Developer of the Original Code is Henrik Lynggaard Hansen
 *
 * Portions created by Henrik Lynggard Hansen are
 * Copyright (C) Henrik Lynggaard Hansen.
 * All Rights Reserved.
 *
 * Contributor(s):
 * Henrik Lynggaard Hansen (Initial Code)
 *
 */
package org.mozilla.translator.datamodel;

import java.util.*;
import javax.swing.*;

/**
 *
 * @author  Henrik Lynggaard
 * @version 4.1
 */
public class MozTreeNode extends AbstractListModel implements Comparable {

    protected List children;
    protected String name;
    protected MozTreeNode parent;
    protected boolean marked;
    
    /** Creates new MozTreeNode */
    public MozTreeNode(String n,MozTreeNode p) 
    {
        name= n;
        parent = p;
        children = new ArrayList();
        marked=false;
    }
    
    public void setName(String name)
    {
        this.name=name;
    }
    
    public String getName()
    {
        return this.name;
    }
    
    public MozTreeNode getParent()
    {
        return parent;
    }
    
    public boolean isMarked()
    {
        return marked;
    }
    
    public void setMarked(boolean m)
    {
        marked=m;
    }
    
    public void addChild(MozTreeNode child)
    {
        children.add(child);
    }
    
    public void removeChild(MozTreeNode child)
    {
        children.remove(child);
    }
    
    public MozTreeNode getChildByName(String name)
    {
        Iterator childIterator;
        MozTreeNode child,result;
        boolean done;
        
        childIterator = children.iterator();
        done=false;
        result=null;
        
        while (!done && childIterator.hasNext())
        {
            child = (MozTreeNode) childIterator.next();
            
            if (name.equals(child.getName()))
            {
                done=true;
                result=child;
            }
        }
        return result;
    }
    
    public Iterator getChildIterator()
    {
        return children.iterator();
    }
    
    public List getAllChildren()
    {
        return children;
    }
    
    public String toString()
    {
        return name;
    }
    
    
    // Methods for the AbstractListModel
    
    public int getSize()
    {
        return children.size();
    }
    
    public Object getElementAt(int index)
    {
        return children.get(index);
    }

    public void fireContentsChanged()
    {
        fireContentsChanged(this,0,children.size());
    }
    
    public Object[] toArray()
    {
        return children.toArray();
    }    

    public int compareTo(final java.lang.Object pl) 
    {
        MozTreeNode other = (MozTreeNode) pl;
        return name.compareTo(other.name);
    }
}