// ----------
// Variable: drag
// The Drag that's currently in process. 
var drag = {
  info: null,
  zIndex: 100
};


// ##########
// Class: Drag (formerly DragInfo)
// Helper class for dragging <Item>s
// 
// ----------
// Constructor: Drag
// Called to create a Drag in response to a jQuery-UI draggable "start" event.
var Drag = function(element, event) {
  this.el = element;
  this.$el = iQ(this.el);
  this.item = Items.item(this.el);
  this.parent = this.item.parent;
  this.startPosition = new Point(event.clientX, event.clientY);
  this.startTime = Utils.getMilliseconds();
  
  this.$el.data('isDragging', true);
  this.item.setZ(999999);
  
  Trenches.activateOthersTrenches(this.el);
  
  // When a tab drag starts, make it the focused tab.
  if(this.item.isAGroup) {
    var tab = Page.getActiveTab();
    if(!tab || tab.parent != this.item) {
      if(this.item._children.length)
        Page.setActiveTab(this.item._children[0]);
    }
  } else {
    Page.setActiveTab(this.item);
  }
};

Drag.prototype = {
  // ----------  
  snap: function(event, ui){
		var bounds = this.item.getBounds();

		// OH SNAP!
		var newRect = Trenches.snap(bounds,true);
		if (newRect) // might be false if no changes were made
			this.item.setBounds(newRect,true);

    return ui;
  },
  
  // ----------  
  // Function: drag
  // Called in response to a jQuery-UI draggable "drag" event.
  drag: function(event, ui) {
//    if(this.item.isAGroup) {
      var bb = this.item.getBounds();
      bb.left = ui.position.left;
      bb.top = ui.position.top;
      this.item.setBounds(bb, true);
      ui = this.snap(event,ui);
//    } else
//      this.item.reloadBounds();
      
    if(this.parent && this.parent.expanded) {
      var now = Utils.getMilliseconds();
      var distance = this.startPosition.distance(new Point(event.clientX, event.clientY));
      if(/* now - this.startTime > 500 ||  */distance > 100) {
        this.parent.remove(this.item);
        this.parent.collapse();
      }
    }
  },

  // ----------  
  // Function: stop
  // Called in response to a jQuery-UI draggable "stop" event.
  stop: function() {
    this.$el.data('isDragging', false);    

    // I'm commenting this out for a while as I believe it feels uncomfortable
    // that groups go away when there is still a tab in them. I do this at
    // the cost of symmetry. -- Aza
    /*
    if(this.parent && !this.parent.locked.close && this.parent != this.item.parent 
        && this.parent._children.length == 1 && !this.parent.getTitle()) {
      this.parent.remove(this.parent._children[0]);
    }*/

    if(this.parent && !this.parent.locked.close && this.parent != this.item.parent 
        && this.parent._children.length == 0 && !this.parent.getTitle()) {
      this.parent.close();
    }
     
    if(this.parent && this.parent.expanded)
      this.parent.arrange();
      
    if(this.item && !this.item.parent) {
      this.item.setZ(drag.zIndex);
      drag.zIndex++;
      
      this.item.reloadBounds();
      this.item.pushAway();
    }
    
    Trenches.disactivate();
    
  }
};
