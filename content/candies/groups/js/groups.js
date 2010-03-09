(function(){

var zIndex = 100;
var isDragging = false;
var $dragged = null;
var timeout = null;

$(".tab").draggable({
  start:function(){
    isDragging = true;
    $dragged = $(this);
  },
  stop: function(){
    isDragging = false;
    $(this).css({zIndex: zIndex});
    $dragged = null;    
    zIndex += 1;
  },
  zIndex: 999,
});

$(".tab").droppable({
  accept: ".tab",
  activeClass: "active",
  hoverClass: "active",
  tolerance: "pointer",
  greedy: true,
  drop: function(e){
    $target = $(e.target);

    // Only drop onto the top z-index
    if( $target.css("zIndex") < $dragged.data("topDropZIndex") ) return;
    $dragged.data("topDropZIndex", $target.css("zIndex") );
    $dragged.data("topDrop", $target);
    
    clearTimeout( timeout );
    var dragged = $dragged;
    var target = $target;
    timeout = setTimeout( function(){
      dragged.removeClass("willGroup")   

      dragged.animate({
        top: target.position().top+10,
        left: target.position().left+10,      
      }, 100)      
    }, 10 );
    
    
  },
  over: function(e){
    $dragged.addClass("willGroup")
    $dragged.data("topDropZIndex", 0);    
  },
  out: function(){
    $dragged.removeClass("willGroup")
  }
})

})();