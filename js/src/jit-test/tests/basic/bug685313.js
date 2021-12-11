
function foo() {
    function D(){}
    arr = [
	   new (function D   (  ) { 
		   D += '' + foo; 
	       }), 
        new D
	   ];
}
foo();
