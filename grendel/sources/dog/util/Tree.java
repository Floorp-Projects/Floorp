/*
 * Tree.java
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Knife.
 *
 * The Initial Developer of the Original Code is dog.
 * Portions created by dog are
 * Copyright (C) 1998 dog <dog@dog.net.uk>. All
 * Rights Reserved.
 *
 * Contributor(s): n/a.
 *
 * You may retrieve the latest version of this package at the knife home page,
 * located at http://www.dog.net.uk/knife/
 */

package dog.util;

import java.util.Vector;

/**
 * A utility class for manipulating objects in a tree structure.
 * <p>
 * This class provides a way to associate arbitrary objects with one another in a hierarchy.
 *
 * @author dog@dog.net.uk
 * @version 1.0final
 */
public final class Tree {

	int nparents = 0;
	Object[] parents;
	int[] nchildren;
	Object[][] children;
	int nelements = 0;
	Object[] elements;
	int[] depths;

	int increment;
	Object LOCK = new Object();

	/**
	 * Constructs an empty tree.
	 */
	public Tree() {
		this(10);
	}
	
	/**
	 * Constructs an empty tree with the specified capacity increment.
	 * @param increment the amount by which the capacity is increased when an array overflows.
	 */
	public Tree(int increment) {
		super();
		this.increment = (increment>0) ? increment : 1;
		clear();
	}

	// Returns the elements index of the specified object, or -1 if no such element exists.
	private int elementIndex(Object object) {
		for (int i=0; i<nelements; i++)
			if (elements[i]==object)
				return i;
		return -1;
	}

	// Returns the parents index of the specified object, or -1 if no such parent exists.
	private int parentIndex(Object object) {
		for (int i=0; i<nparents; i++)
			if (parents[i]==object)
				return i;
		return -1;
	}

	/**
	 * Returns the parent of the specified object,
	 * or null if the object is a root or not a child in the tree.
	 */
	public Object getParent(Object child) {
		synchronized (LOCK) {
			return parentOf(child);
		}
	}

	private Object parentOf(Object child) {
		for (int p = 0; p<nparents; p++) {
			for (int c = 0; c<nchildren[p]; c++)
				if (children[p][c]==child)
					return parents[p];
		}
		return null;
	}

	/**
	 * Returns an array of children of the specified object,
	 * or an empty array if the object is not a parent in the tree.
	 */
	public Object[] getChildren(Object parent) {
		synchronized (LOCK) {
			int p = parentIndex(parent);
			if (p>-1) {
				Object[] c = new Object[nchildren[p]];
				System.arraycopy(children[p], 0, c, 0, nchildren[p]);
				return c;
			}
		}
		return new Object[0];
	}

	/**
	 * Returns the number of children of the specified object.
	 */
	public int getChildCount(Object parent) {
		synchronized (LOCK) {
			int p = parentIndex(parent);
			if (p>-1)
				return nchildren[p];
		}
		return 0;
	}

	/**
	 * Indicates whether the specified object is contained in the tree.
	 */
	public boolean contains(Object object) {
		synchronized (LOCK) {
			return (elementIndex(object)>-1);
		}
	}

	private void ensureRootCapacity(int amount) {
		// calculate the amount by which to increase arrays
		int block = ((int)(Math.max(Math.ceil((double)amount/(double)increment), 1)))*increment;
		// ensure capacity of elements
		if (elements.length<nelements+amount) {
			int len = elements.length;
			Object[] newelements = new Object[len+block];
			System.arraycopy(elements, 0, newelements, 0, nelements);
			elements = newelements;
			int[] newdepths = new int[len+block];
			System.arraycopy(depths, 0, newdepths, 0, nelements);
			depths = newdepths;
		}
	}

	private void ensureParentCapacity(int amount) {
		// calculate the amount by which to increase arrays
		int block = ((int)(Math.max(Math.ceil((double)amount/(double)increment), 1)))*increment;
		// ensure capacity of parents (and hence children in parent dimension)
		if (parents.length<nparents+1) {
			Object[] newparents = new Object[parents.length+increment];
			System.arraycopy(parents, 0, newparents, 0, nparents);
			parents = newparents;
			Object[][] newchildren = new Object[parents.length+increment][];
			System.arraycopy(children, 0, newchildren, 0, nparents);
			children = newchildren;
			int[] newnchildren = new int[parents.length+increment];
			System.arraycopy(nchildren, 0, newnchildren, 0, nparents);
			nchildren = newnchildren;
		}
		// ensure capacity of elements
		if (elements.length<nelements+amount) {
			int len = elements.length;
			Object[] newelements = new Object[len+block];
			System.arraycopy(elements, 0, newelements, 0, nelements);
			elements = newelements;
			int[] newdepths = new int[len+block];
			System.arraycopy(depths, 0, newdepths, 0, nelements);
			depths = newdepths;
		}
	}

	private void ensureChildCapacity(int amount, int parent) {
		// calculate the amount by which to increase arrays
		int block = ((int)(Math.max(Math.ceil((double)amount/(double)increment), 1)))*increment;
		// ensure capacity of children for this parent
		if (children[parent].length<nchildren[parent]+amount) {
			Object[] newchildren = new Object[children[parent].length+block];
			System.arraycopy(children[parent], 0, newchildren, 0, nchildren[parent]);
			children[parent] = newchildren;
		}
		// ensure capacity of elements
		if (elements.length<nelements+amount) {
			int len = elements.length;
			Object[] newelements = new Object[len+block];
			System.arraycopy(elements, 0, newelements, 0, nelements);
			elements = newelements;
			int[] newdepths = new int[len+block];
			System.arraycopy(depths, 0, newdepths, 0, nelements);
			depths = newdepths;
		}
	}

	/**
	 * Adds a root to the tree.
	 * Returns the root if it was added.
	 */
	public void add(Object object) {
		if (object==null)
			throw new NullPointerException("cannot add null to a tree");
		synchronized (LOCK) {
			int e;
			if ((e = elementIndex(object))>-1) // if we already have the object
				removeElement(e); // remove it
			ensureRootCapacity(1);
			elements[nelements] = object;
			depths[nelements] = 1;
			nelements++;
		}
	}

	/**
	 * Adds roots to the tree.
	 */
	public void add(Object[] objects) {
		synchronized (LOCK) {
			for (int i=0; i<objects.length; i++) {
				if (objects[i]==null)
					throw new NullPointerException("cannot add null to a tree");
				int e;
				if ((e = elementIndex(objects[i]))>-1) // if we already have the object
					removeElement(e); // remove it
			}
			ensureRootCapacity(objects.length);
			System.arraycopy(objects, 0, elements, nelements, objects.length);
			for (int i=nelements; i<nelements+objects.length; i++)
				depths[i] = 1;
			nelements += objects.length;
		}
	}

	/**
	 * Adds a child to the specified parent in the tree.
	 * @throws IllegalArgumentException if the parent does not exist in the tree or the child is the parent.
	 */
	public void add(Object parent, Object child) {
		if (parent==null)
			throw new NullPointerException("null parent specified");
		if (child==null)
			throw new NullPointerException("cannot add null to a tree");
		if (child==parent)
			throw new IllegalArgumentException("cannot add child to itself");
		synchronized (LOCK) {
			int p = parentIndex(parent), pe = -1;
			if (p<0) { // does it exist in the tree?
				if ((pe = elementIndex(parent))<0)
					throw new IllegalArgumentException("parent does not exist");
			} // so we'll add it to parents
			int e;
			if ((e = elementIndex(child))>-1) // if the child is already an element
				removeElement(e);

			if (p<0) { // add the parent to parents
				p = nparents;
				ensureParentCapacity(1);
				parents[p] = parent;
				children[p] = new Object[increment];
				nchildren[p] = 0;
				nparents++;
			}
			ensureChildCapacity(1, p);
			children[p][nchildren[p]] = child;

			// insert the child into elements and set its depth
			int depth = depth(parent);
			int off = pe+nchildren[p]+1;
			int len = nelements-off;
			Object[] buffer = new Object[len];
			System.arraycopy(elements, off, buffer, 0, len);
			elements[off] = child;
			System.arraycopy(buffer, 0, elements, off+1, len);
			int[] dbuffer = new int[len];
			System.arraycopy(depths, off, dbuffer, 0, len);
			depths[off] = depth+1;
			System.arraycopy(dbuffer, 0, depths, off+1, len);
			nelements++;

			nchildren[p]++;
		}
	}

	/**
	 * Adds children to the specified parent in the tree.
	 * @throws IllegalArgumentException if the parent does not exist in the tree or one of the children is the parent.
	 */
	public void add(Object parent, Object[] children) {
		if (parent==null)
			throw new NullPointerException("null parent specified");
		synchronized (LOCK) {
			int p = parentIndex(parent), pe = -1;
			if (p<0) { // does it exist in the tree?
				if ((pe = elementIndex(parent))<0)
					throw new IllegalArgumentException("parent does not exist");
			} // so we'll add it to parents
			for (int i=0; i<children.length; i++) {
				if (children[i]==null)
					throw new NullPointerException("cannot add null to a tree");
				if (children[i]==parent)
					throw new IllegalArgumentException("cannot add child to itself");
				int e;
				if ((e = elementIndex(children[i]))>-1) // if we already have the object
					removeElement(e); // remove it
			}
			if (p<0) { // add the parent to parents
				p = nparents;
				ensureParentCapacity(1);
				parents[p] = parent;
				this.children[p] = new Object[Math.max(increment, (children.length/increment)*increment)];
				nchildren[p] = 0;
				nparents++;
			}
			ensureChildCapacity(children.length, p);
			System.arraycopy(children, 0, this.children[p], nchildren[p], children.length);

			// insert the children into elements and set their depths
			int depth = depth(parent);
			int off = pe+nchildren[p]+1;
			int len = nelements-off;
			Object[] buffer = new Object[len];
			System.arraycopy(elements, off, buffer, 0, len);
			System.arraycopy(children, 0, elements, off, children.length);
			System.arraycopy(buffer, 0, elements, off+children.length, len);

			int[] dbuffer = new int[len];
			System.arraycopy(depths, off, dbuffer, 0, len);
			for (int i=off; i<off+children.length; i++)
				depths[i] = depth+1;
			System.arraycopy(dbuffer, 0, depths, off+children.length, len);
			nelements += children.length;

			nchildren[p] += children.length;
		}
	}

	// Returns the depth of an object in the tree
	private int depth(Object object) {
		Object parent = parentOf(object);
		if (parent==null)
			return 1;
		else
			return depth(parent)+1;
	}

	/**
	 * Removes the specified object and all its children from the tree.
	 * Computationally pretty expensive.
	 */
	public void remove(Object object) {
		if (object==null)
			return; // i mean, really...
		synchronized (LOCK) {
			int e = elementIndex(object);
			if (e>-1)
				removeElement(e);
		}
	}

	void removeElement(int e) {
		int len = 0;
        // does this element have children?
		for (int p=0; p<nparents; p++) {
			if (parents[p]==elements[e]) {
				int nc = nchildren[p];
                // remove the children of this parent first
                Object[] pchildren = new Object[nc];
				System.arraycopy(children[p], 0, pchildren, 0, nc);
				for (int c=0; c<nc; c++) {
					int ce = elementIndex(pchildren[c]);
					if (ce>-1)
						removeElement(ce);
				}
                // remove the parent
				if ((len = nparents-p-1)>0) {
					System.arraycopy(parents, p+1, parents, p, len);
					System.arraycopy(children, p+1, children, p, len);
					System.arraycopy(nchildren, p+1, nchildren, p, len);
				}
				nparents--; p--;
			} else {
				// is this element a child of another parent?
				for (int c=0; c<nchildren[p]; c++) {
					if (children[p][c]==elements[e]) {
						// remove this element from the children array
						if ((len = nchildren[p]-c-1)>0) {
							System.arraycopy(children[p], c+1, children[p], c, len);
						}
						nchildren[p]--; c--;
					}
				}
			}
		}
		if ((len = nelements-e-1)>0) {
			System.arraycopy(elements, e+1, elements, e, len);
			System.arraycopy(depths, e+1, depths, e, len);
		}
		nelements--;
	}
	
	/**
	 * Removes the specified objects and all their children from the tree.
	 * @throws NullPointerException if an object does not exist in the tree.
	 */
	public void remove(Object[] objects) {
		for (int i=0; i<objects.length; i++)
			remove(objects[i]);
	}
	
	/**
	 * Recursively removes the specified object's children from the tree.
	 * @throws NullPointerException if the object does not exist in the tree.
	 */
	public void removeChildren(Object parent) {
		synchronized (LOCK) {
			removeChildrenImpl(parent);
		}
	}

	void removeChildrenImpl(Object parent) {
		int p = parentIndex(parent);
		if (p>-1) {
			for (int c=0; c<nchildren[p]; c++)
				removeChildrenImpl(children[p][c]);
			children[p] = new Object[increment];
			nchildren[p] = 0;
		}
	}

	/**
	 * Removes all objects from the tree.
	 */
	public void clear() {
		synchronized (LOCK) {
			parents = new Object[increment];
			nparents = 0;
			nchildren = new int[increment];
			for (int i=0; i<increment; i++)
				nchildren[i] = 0;
			children = new Object[increment][increment];
			elements = new Object[increment];
			depths = new int[increment];
			nelements = 0;
		}
	}

	/**
	 * Returns the depth of the specified object in the tree.
	 * Roots have a depth of 1.
	 */
	public int getDepth(Object object) {
		synchronized (LOCK) {
			int e = elementIndex(object);
			if (e==-1) {
				//System.out.println("Warning: "+object+" not found");
				//new Throwable().printStackTrace();
				return -1;
			}
			return depths[e];
		}
	}

	/**
	 * Returns the roots in this tree.
	 */
	public Object[] getRoots() {
		synchronized (LOCK) {
			Vector v = new Vector();
			for (int i=0; i<nelements; i++) {
				if (depths[i]==1)
					v.addElement(elements[i]);
			}
			Object[] roots = new Object[v.size()];
			v.copyInto(roots);
			return roots;
		}
	}

	/**
	 * Returns the number of roots in this tree.
	 */
	public int getRootCount() {
		synchronized (LOCK) {
			Vector v = new Vector();
			for (int i=0; i<nelements; i++) {
				if (depths[i]==1)
					v.addElement(elements[i]);
			}
			return v.size();
		}
	}

	/**
	 * Returns all the objects in this tree in depth-first order.
	 */
	public Object[] getTree() {
		synchronized (LOCK) {
			Object[] objects = new Object[nelements];
			System.arraycopy(elements, 0, objects, 0, nelements);
			return objects;
		}
	}

	/**
	 * Returns the number of objects in this tree.
	 */
	public int getTreeCount() {
		return nelements;
	}

	/**
	 * Indicates whether there are any children in this tree.
	 */
	public boolean hasChildren() {
		synchronized (LOCK) {
			for (int p=0; p<nparents; p++)
				if (nchildren[p]>0)
					return true;
		}
		return false;
	}
	
	/**
	 * Sorts the objects in the tree according to the specified object collator.
	 * This has no effect if a collator has not been defined.
	 */
	public void sort(ObjectCollator collator) {
		if (collator==null) throw new NullPointerException("null collator");
		synchronized (LOCK) {
			Sorter sorter = this.new Sorter(collator);
			elements = sorter.newelements;
			depths = sorter.newdepths;
		}
	}

	class Sorter {

		Object[] newelements;
		int[] newdepths;
		int count = 0;

		Sorter(ObjectCollator collator) {
			// first sort the roots
			Vector v = new Vector();
			for (int i=0; i<nelements; i++) {
				if (depths[i]==1)
					v.addElement(elements[i]);
			}
			Object[] roots = new Object[v.size()];
			v.copyInto(roots);
			sort(roots, 0, roots.length-1, collator);
			// now sort the children of each parent
			for (int p=0; p<nparents; p++)
				sort(children[p], 0, nchildren[p]-1, collator);
			// now create a new elements array positioning the elements according to the new order
			newelements = new Object[elements.length];
			newdepths = new int[elements.length];
			for (int r=0; r<roots.length; r++) {
				newelements[count] = roots[r];
				newdepths[count] = depths[elementIndex(roots[r])];
				count++;
				appendChildren(roots[r]);
			}
		}

		void appendChildren(Object parent) {
			for (int p=0; p<nparents; p++) {
				if (parent==parents[p]) {
					for (int c=0; c<nchildren[p]; c++) {
						newelements[count] = children[p][c];
						newdepths[count] = getDepth(children[p][c]);
						count++;
						appendChildren(children[p][c]);
					}
					break;
				}
			}
		}
		
	}

	// Quicksort algorithm.
	static void sort(Object[] objects, int first, int last, ObjectCollator collator) {
		int lo = first, hi = last;
		Object mid;
		if (last>first) {
			mid = objects[(first+last)/2];
			while (lo<=hi) {
				while (lo<last && collator.compare(objects[lo], mid)<0)
					lo += 1;
				while (hi>first && collator.compare(objects[hi], mid)>0)
					hi -= 1;
				if (lo<=hi) { // swap
					Object tmp = objects[lo];
					objects[lo] = objects[hi];
					objects[hi] = tmp;
					lo += 1;
					hi -= 1;
				}
			}
			if (first<hi)
				sort(objects, first, hi, collator);
			if (lo<last)
				sort(objects, lo, last, collator);
		}
	}

	public void printArray(String name, Object[] array, int count) {
		System.out.println(name+": length="+array.length+", count="+count);
		for (int i=0; i<count; i++)
			System.out.println("\t"+array[i]);
	}

	public void printElements() {
        System.out.println("elements=");
		for (int i=0; i<nelements; i++) {
			StringBuffer buffer = new StringBuffer();
			buffer.append(Integer.toString(depths[i]));
			for (int j=0; j<depths[i]; j++)
				buffer.append("  ");
			buffer.append(elements[i]);
			System.out.println(buffer.toString());
		}
		System.out.println("tree=");
		for (int p=0; p<nparents; p++) {
			System.out.println(parents[p]);
			for (int c=0; c<nchildren[p]; c++)
				System.out.println("  "+children[p][c]);
		}
	}

	// -- Utility methods --

	public String toString() {
		StringBuffer buffer = new StringBuffer();
		buffer.append(getClass().getName());
		buffer.append("[");
		buffer.append("nelements="+nelements);
		buffer.append(",elements.length="+elements.length);
		buffer.append(",depths.length="+depths.length);
		buffer.append(",nparents="+nparents);
		buffer.append(",parents.length="+parents.length);
		buffer.append("]");
		return buffer.toString();
	}

}
