TryInWhile( new TryObject( "hello", ThrowException, true ) );
function TryObject( value, throwFunction, result ) {
  this.thrower=throwFunction
}
function ThrowException() TryInWhile(1);
function TryInWhile( object ) {
    try {
      object.thrower()
    } catch ( e ) {
    }  
}
