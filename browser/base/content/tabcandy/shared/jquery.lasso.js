// By Aza Raskin, 2010

function Lasso(options){ this.init(options); }
Lasso.prototype = {
  init: function(options){
    var canvas = document.createElement("canvas");
    var w = window.innerWidth;
    var h = window.innerHeight;
    
    this._selector    = options.targets;
    this._container   = options.container || "body";
    this._fillColor   = options.fillColor || "rgba(0,0,255,.1)";
    this._strokeColor = options.strokeColor || "rgba(0,0,255,.4)";
    this._strokeWidth = options.strokeWidth || 1;
    this._onSelect    = options.onSelect || function(){};
    this._onStart     = options.onStart || function(){};
    this._onMove      = options.onMove;
    this._acceptMouseDown = options.acceptMouseDown || function(){ return true; };
    
    if( options.fill != false ) this._fill = true;
    else this._fill = false;
      
    this._lastPos = null;
    this._isSelecting = false;
        
    $(canvas)
      .attr({width:w,height:h})
      .css({position:"fixed", top:0, left:0, width:w, height:h, zIndex:9999})
      .appendTo("body")
      .hide();
    this.canvas = canvas;
    this.ctx = canvas.getContext("2d");

    this.on();
  },
  
  on: function(){
    var self = this;
    $(this._container)
      .mousedown( function(e){
        if(Utils.isRightClick(e)
            || $(e.target).is(self._selector) 
            || $(e.target).parent(self._selector).length > 0 
            || !self._acceptMouseDown(e)) 
          return;
          
        self.startSelection();
        return false;
      })
      .mouseup( function(){self.stopSelection()} )
  },
  
  startSelection: function(){
    var self = this;
    this._isSelecting = true;
    this._onStart();
    
    this._draw = function(e){
      var pos = {x:e.clientX, y:e.clientY};
      self._updatePos( pos );
    };
    this.ctx.beginPath();
    this.ctx.fillStyle = this._fillColor;
    this.ctx.strokeStyle = this._strokeColor;
    this.ctx.lineWidth = this._strokeWidth;
    $(this.canvas).mousemove(this._draw);
    $(this.canvas).show();
    

  },
  
  stopSelection: function(){
    $(this.canvas).unbind("mousemove", this._draw);
    this._isSelecting = false;
    this._hitTest();         
    $(this.canvas).hide();
    this._onSelect( this.getSelectedElements(), this._lastPos )
    this._clear();
    
  },
  
  _updatePos: function(pos){
    var self = this;

    if( ! self._isSelecting ) return;
    
    if( self._lastPos == null ){
      self._lastPos = pos;
      return;
    }
        
    with(this.ctx){
      clearRect(0,0,this.canvas.width,this.canvas.height)   
      lineTo(pos.x, pos.y);
      stroke();
      if( self._fill ) fill();
    }
    
    self._lastPos = pos;
    
    if(self._onMove) {
      this._hitTest();
      this._onMove(this.getSelectedElements());
    }
  },
  
  _clear: function(){
    // A fast way of clearing the canvas     
    this.canvas.setAttribute('width', this.canvas.width);    
    this._lastPos = null;
  },
  
  _hitTest: function(){
    var w = this.canvas.width;
    var h = this.canvas.height;
    var data = this.ctx.getImageData(0,0, w, h).data;
    $(this.canvas).hide();
    
    var hitElements = [];
    
    // can make this bounding much faster
    for( var x=0; x<this.canvas.width; x+=10){
      for( var y=0; y<this.canvas.height; y+=10){
        var index = (y*w+x)*4+3;
        // If transparent (i.e., empty)
        if( data[index] != 0 ){
          var hit = document.elementFromPoint(x,y);
          var desired = null;
          if( $(hit).is(this._selector) )
            desired = hit;
          else if( $(hit).parent(this._selector).length > 0 )
            desired = $(hit).parent(this._selector).get(0);
          if( desired && hitElements.indexOf( desired ) == -1 )
            hitElements.push( desired );
          
        }
      }
    }

    $(this.canvas).show();
    this.selectedEls = hitElements;
  },
  
  getSelectedElements: function(){
    return this.selectedEls;
  }
}
