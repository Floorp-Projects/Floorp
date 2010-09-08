/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2007, Parakey Inc.
 * All rights reserved.
 * 
 * Redistribution and use of this software in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the
 *   following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the
 *   following disclaimer in the documentation and/or other
 *   materials provided with the distribution.
 * 
 * * Neither the name of Parakey Inc. nor the names of its
 *   contributors may be used to endorse or promote products
 *   derived from this software without specific prior
 *   written permission of Parakey Inc.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Creator:
 *  Joe Hewitt
 * Contributors
 *  John J. Barton (IBM Almaden)
 *  Jan Odvarko (Mozilla Corp.)
 *  Max Stepanov (Aptana Inc.)
 *  Rob Campbell (Mozilla Corp.)
 *  Hans Hillen (Paciello Group, Mozilla)
 *  Curtis Bartley (Mozilla Corp.)
 *  Mike Collins (IBM Almaden)
 *  Kevin Decker
 *  Mike Ratcliffe (Comartis AG)
 *  Hernan Rodríguez Colmeiro
 *  Austin Andrews
 *  Christoph Dorn
 *  Steven Roussey (AppCenter Inc, Network54)
 */

///////////////////////////////////////////////////////////////////////////
//// InsideOutBox

/**
 * InsideOutBoxView is a simple interface definition for views implementing
 * InsideOutBox controls. All implementors must define these methods.
 * Implemented in InspectorUI.
 */

/*
InsideOutBoxView = {
  //
   * Retrieves the parent object for a given child object.
   * @param aChild
   *        The child node to retrieve the parent object for.
   * @returns a DOM node | null
   //
  getParentObject: function(aChild) {},

  //
   * Retrieves a given child node.
   *
   * If both index and previousSibling are passed, the implementation
   * may assume that previousSibling will be the return for getChildObject
   * with index-1.
   * @param aParent
   *        The parent object of the child object to retrieve.
   * @param aIndex
   *        The index of the child object to retrieve from aParent.
   * @param aPreviousSibling
   *        The previous sibling of the child object to retrieve.
   *        Supercedes aIndex.
   * @returns a DOM object | null
   //
  getChildObject: function(aParent, aIndex, aPreviousSibling) {},

  //
   * Renders the HTML representation of the object. Should return an HTML
   * object which will be displayed to the user.
   * @param aObject
   *        The object to create the box object for.
   * @param aIsRoot
   *        Is the object the root object. May not be used in all
   *        implementations.
   * @returns an object box | null
   //
  createObjectBox: function(aObject, aIsRoot) {},

  //
  * Convenience wrappers for classList API.
  * @param aObject
  *        DOM node to query/set.
  * @param aClassName
  *        String containing the class name to query/set.
  //
  hasClass: function(aObject, aClassName) {},
  addClass: function(aObject, aClassName) {},
  removeClass: function(aObject, aClassName) {}
};
*/

/**
 * Creates a tree based on objects provided by a separate "view" object.
 *
 * Construction uses an "inside-out" algorithm, meaning that the view's job is
 * first to tell us the ancestry of each object, and secondarily its
 * descendants.
 *
 * Constructor
 * @param aView
 *        The view requiring the InsideOutBox.
 * @param aBox
 *        The box object containing the InsideOutBox. Required to add/remove
 *        children during box manipulation (toggling opened or closed).
 */
function InsideOutBox(aView, aBox)
{
  this.view = aView;
  this.box = aBox;

  this.rootObject = null;

  this.rootObjectBox = null;
  this.selectedObjectBox = null;
  this.highlightedObjectBox = null;
  this.scrollIntoView = false;
};

InsideOutBox.prototype =
{
  /**
   * Highlight the given object node in the tree.
   * @param aObject
   *        the object to highlight.
   * @returns objectBox
   */
  highlight: function IOBox_highlight(aObject)
  {
    let objectBox = this.createObjectBox(aObject);
    this.highlightObjectBox(objectBox);
    return objectBox;
  },

  /**
   * Open the given object node in the tree.
   * @param aObject
   *        The object node to open.
   * @returns objectBox
   */
  openObject: function IOBox_openObject(aObject)
  {
    let object = aObject;
    let firstChild = this.view.getChildObject(object, 0);
    if (firstChild)
      object = firstChild;

    return this.openToObject(object);
  },

  /**
   * Open the tree up to the given object node.
   * @param aObject
   *        The object in the tree to open to.
   * @returns objectBox
   */
  openToObject: function IOBox_openToObject(aObject)
  {
    let objectBox = this.createObjectBox(aObject);
    this.openObjectBox(objectBox);
    return objectBox;
  },

  /**
   * Select the given object node in the tree.
   * @param aObject
   *        The object node to select.
   * @param makeBoxVisible
   *        Boolean. Open the object box in the tree?
   * @param forceOpen
   *        Force the object box open by expanding all elements in the tree?
   * @param scrollIntoView
   *        Scroll the objectBox into view?
   * @returns objectBox
   */
  select:
  function IOBox_select(aObject, makeBoxVisible, forceOpen, scrollIntoView)
  {
    let objectBox = this.createObjectBox(aObject);
    this.selectObjectBox(objectBox, forceOpen);
    if (makeBoxVisible) {
      this.openObjectBox(objectBox);
      if (scrollIntoView) {
        objectBox.scrollIntoView(true);
      }
    }
    return objectBox;
  },

  /**
   * Expands/contracts the given object, depending on its state.
   * @param aObject
   *        The tree node to expand/contract.
   */
  toggleObject: function IOBox_toggleObject(aObject)
  {
    let box = this.createObjectBox(aObject);
    if (!(this.view.hasClass(box, "open")))
      this.expandObjectBox(box);
    else
      this.contractObjectBox(box);
  },

  /**
   * Expand the given object in the tree.
   * @param aObject
   *        The tree node to expand.
   */
  expandObject: function IOBox_expandObject(aObject)
  {
    let objectBox = this.createObjectBox(aObject);
    if (objectBox)
      this.expandObjectBox(objectBox);
  },

  /**
   * Contract the given object in the tree.
   * @param aObject
   *        The tree node to contract.
   */
  contractObject: function IOBox_contractObject(aObject)
  {
    let objectBox = this.createObjectBox(aObject);
    if (objectBox)
      this.contractObjectBox(objectBox);
  },

  /**
   * General method for iterating over an object's ancestors and performing
   * some function.
   * @param aObject
   *        The object whose ancestors we wish to iterate over.
   * @param aCallback
   *        The function to call with the object as argument.
   */

  iterateObjectAncestors: function IOBox_iterateObjectAncesors(aObject, aCallback)
  {
    let object = aObject;
    if (!(aCallback && typeof(aCallback) == "function")) {
      this.view._log("Illegal argument in IOBox.iterateObjectAncestors");
      return;
    }
    while ((object = this.getParentObjectBox(object)))
      aCallback(object);
  },

  /**
   * Highlight the given objectBox in the tree.
   * @param aObjectBox
   *        The objectBox to highlight.
   */
  highlightObjectBox: function IOBox_highlightObjectBox(aObjectBox)
  {
    let self = this;

    if (!aObjectBox)
      return;

    if (this.highlightedObjectBox) {
      this.view.removeClass(this.highlightedObjectBox, "highlighted");
      this.iterateObjectAncestors(this.highlightedObjectBox, function (box) {
        self.view.removeClass(box, "highlightOpen");
      });
    }

    this.highlightedObjectBox = aObjectBox;

    this.view.addClass(aObjectBox, "highlighted");
    this.iterateObjectAncestors(this.highlightedObjectBox, function (box) {
      self.view.addClass(box, "highlightOpen");
    });

    aObjectBox.scrollIntoView(true);
  },

  /**
   * Select the given objectBox in the tree, forcing it to be open if necessary.
   * @param aObjectBox
   *        The objectBox to select.
   * @param forceOpen
   *        Force the box (subtree) to be open?
   */
  selectObjectBox: function IOBox_selectObjectBox(aObjectBox, forceOpen)
  {
    let isSelected = this.selectedObjectBox &&
      aObjectBox == this.selectedObjectBox;

    // aObjectBox is already selected, return
    if (isSelected)
      return;

    if (this.selectedObjectBox)
      this.view.removeClass(this.selectedObjectBox, "selected");

    this.selectedObjectBox = aObjectBox;

    if (aObjectBox) {
      this.view.addClass(aObjectBox, "selected");

      // Force it open the first time it is selected
      if (forceOpen)
        this.expandObjectBox(aObjectBox, true);
    }
  },

  /**
   * Open the ancestors of the given object box.
   * @param aObjectBox
   *        The object box to open.
   */
  openObjectBox: function IOBox_openObjectBox(aObjectBox)
  {
    if (!aObjectBox)
      return;

    let self = this;
    this.iterateObjectAncestors(aObjectBox, function (box) {
      self.view.addClass(box, "open");
      let labelBox = box.querySelector(".nodeLabelBox");
      if (labelBox)
        labelBox.setAttribute("aria-expanded", "true");
   });
  },

  /**
   * Expand the given object box.
   * @param aObjectBox
   *        The object box to expand.
   */
  expandObjectBox: function IOBox_expandObjectBox(aObjectBox)
  {
    let nodeChildBox = this.getChildObjectBox(aObjectBox);

    // no children means nothing to expand, return
    if (!nodeChildBox)
      return;

    if (!aObjectBox.populated) {
      let firstChild = this.view.getChildObject(aObjectBox.repObject, 0);
      this.populateChildBox(firstChild, nodeChildBox);
    }
    let labelBox = aObjectBox.querySelector(".nodeLabelBox");
    if (labelBox)
      labelBox.setAttribute("aria-expanded", "true");
    this.view.addClass(aObjectBox, "open");
  },

  /**
   * Contract the given object box.
   * @param aObjectBox
   *        The object box to contract.
   */
  contractObjectBox: function IOBox_contractObjectBox(aObjectBox)
  {
    this.view.removeClass(aObjectBox, "open");
    let nodeLabel = aObjectBox.querySelector(".nodeLabel");
    let labelBox = nodeLabel.querySelector(".nodeLabelBox");
    if (labelBox)
      labelBox.setAttribute("aria-expanded", "false");
  },

  /**
   * Toggle the given object box, forcing open if requested.
   * @param aObjectBox
   *        The object box to toggle.
   * @param forceOpen
   *        Force the objectbox open?
   */
  toggleObjectBox: function IOBox_toggleObjectBox(aObjectBox, forceOpen)
  {
    let isOpen = this.view.hasClass(aObjectBox, "open");

    if (!forceOpen && isOpen)
      this.contractObjectBox(aObjectBox);
    else if (!isOpen)
      this.expandObjectBox(aObjectBox);
  },

  /**
   * Creates all of the boxes for an object, its ancestors, and siblings.
   * @param aObject
   *        The tree node to create the object boxes for.
   * @returns anObjectBox or null
   */
  createObjectBox: function IOBox_createObjectBox(aObject)
  {
    if (!aObject)
      return null;

    this.rootObject = this.getRootNode(aObject) || aObject;

    // Get or create all of the boxes for the target and its ancestors
    let objectBox = this.createObjectBoxes(aObject, this.rootObject);

    if (!objectBox)
      return null;

    if (aObject == this.rootObject)
      return objectBox;

    return this.populateChildBox(aObject, objectBox.parentNode);
  },

  /**
   * Creates all of the boxes for an object, its ancestors, and siblings up to
   * a root.
   * @param aObject
   *        The tree's object node to create the object boxes for.
   * @param aRootObject
   *        The root object at which to stop building object boxes.
   * @returns an object box or null
   */
  createObjectBoxes: function IOBox_createObjectBoxes(aObject, aRootObject)
  {
    if (!aObject)
      return null;

    if (aObject == aRootObject) {
      if (!this.rootObjectBox || this.rootObjectBox.repObject != aRootObject) {
        if (this.rootObjectBox) {
          try {
            this.box.removeChild(this.rootObjectBox);
          } catch (exc) {
            InspectorUI._log("this.box.removeChild(this.rootObjectBox) FAILS " +
              this.box + " must not contain " + this.rootObjectBox);
          }
        }

        this.highlightedObjectBox = null;
        this.selectedObjectBox = null;
        this.rootObjectBox = this.view.createObjectBox(aObject, true);
        this.box.appendChild(this.rootObjectBox);
      }
      return this.rootObjectBox;
    }

    let parentNode = this.view.getParentObject(aObject);
    let parentObjectBox = this.createObjectBoxes(parentNode, aRootObject);

    if (!parentObjectBox)
      return null;

    let parentChildBox = this.getChildObjectBox(parentObjectBox);

    if (!parentChildBox)
      return null;

    let childObjectBox = this.findChildObjectBox(parentChildBox, aObject);

    return childObjectBox ? childObjectBox
      : this.populateChildBox(aObject, parentChildBox);
  },

  /**
   * Locate the object box for a given object node.
   * @param aObject
   *        The given object node in the tree.
   * @returns an object box or null.
   */
  findObjectBox: function IOBox_findObjectBox(aObject)
  {
    if (!aObject)
      return null;

    if (aObject == this.rootObject)
      return this.rootObjectBox;

    let parentNode = this.view.getParentObject(aObject);
    let parentObjectBox = this.findObjectBox(parentNode);
    if (!parentObjectBox)
      return null;

    let parentChildBox = this.getChildObjectBox(parentObjectBox);
    if (!parentChildBox)
      return null;

    return this.findChildObjectBox(parentChildBox, aObject);
  },

  getAncestorByClass: function IOBox_getAncestorByClass(node, className)
  {
    for (let parent = node; parent; parent = parent.parentNode) {
      if (this.view.hasClass(parent, className))
        return parent;
    }

    return null;
  },

  /**
   * We want all children of the parent of repObject.
   */
  populateChildBox: function IOBox_populateChildBox(repObject, nodeChildBox)
  {
    if (!repObject)
      return null;

    let parentObjectBox = this.getAncestorByClass(nodeChildBox, "nodeBox");

    if (parentObjectBox.populated)
      return this.findChildObjectBox(nodeChildBox, repObject);

    let lastSiblingBox = this.getChildObjectBox(nodeChildBox);
    let siblingBox = nodeChildBox.firstChild;
    let targetBox = null;
    let view = this.view;
    let targetSibling = null;
    let parentNode = view.getParentObject(repObject);

    for (let i = 0; 1; ++i) {
      targetSibling = view.getChildObject(parentNode, i, targetSibling);
      if (!targetSibling)
        break;

      // Check if we need to start appending, or continue to insert before
      if (lastSiblingBox && lastSiblingBox.repObject == targetSibling)
        lastSiblingBox = null;

      if (!siblingBox || siblingBox.repObject != targetSibling) {
        let newBox = view.createObjectBox(targetSibling);
        if (newBox) {
          if (lastSiblingBox)
            nodeChildBox.insertBefore(newBox, lastSiblingBox);
          else
            nodeChildBox.appendChild(newBox);
        }

        siblingBox = newBox;
      }

      if (targetSibling == repObject)
        targetBox = siblingBox;

      if (siblingBox && siblingBox.repObject == targetSibling)
        siblingBox = siblingBox.nextSibling;
    }

    if (targetBox)
      parentObjectBox.populated = true;

    return targetBox;
  },

  /**
   * Get the parent object box of a given object box.
   * @params aObjectBox
   *         The object box of the parent.
   * @returns an object box or null
   */
  getParentObjectBox: function IOBox_getParentObjectBox(aObjectBox)
  {
    let parent = aObjectBox.parentNode ? aObjectBox.parentNode.parentNode : null;
    return parent && parent.repObject ? parent : null;
  },

  /**
   * Get the child object box of a given object box.
   * @param aObjectBox
   *        The object box whose child you want.
   * @returns an object box or null
   */
  getChildObjectBox: function IOBox_getChildObjectBox(aObjectBox)
  {
    return aObjectBox.querySelector(".nodeChildBox");
  },

  /**
   * Find the child object box for a given repObject within the subtree
   * rooted at aParentNodeBox.
   * @param aParentNodeBox
   *        root of the subtree in which to search for repObject.
   * @param aRepObject
   *        The object you wish to locate in the subtree.
   * @returns an object box or null
   */
  findChildObjectBox: function IOBox_findChildObjectBox(aParentNodeBox, aRepObject)
  {
    let childBox = aParentNodeBox.firstChild;
    while (childBox) {
      if (childBox.repObject == aRepObject)
        return childBox;
      childBox = childBox.nextSibling;
    }
    return null; // not found
  },

  /**
   * Determines if the given node is an ancestor of the current root.
   * @param aNode
   *        The node to look for within the tree.
   * @returns boolean
   */
  isInExistingRoot: function IOBox_isInExistingRoot(aNode)
  {
    let parentNode = aNode;
    while (parentNode && parentNode != this.rootObject) {
      parentNode = this.view.getParentObject(parentNode);
    }
    return parentNode == this.rootObject;
  },

  /**
   * Get the root node of a given node.
   * @param aNode
   *        The node whose root you wish to retrieve.
   * @returns a root node or null
   */
  getRootNode: function IOBox_getRootNode(aNode)
  {
    let node = aNode;
    let tmpNode;
    while ((tmpNode = this.view.getParentObject(node)))
      node = tmpNode;

    return node;
  },
};
