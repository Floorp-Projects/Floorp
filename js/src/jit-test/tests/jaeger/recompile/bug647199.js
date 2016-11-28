TryInWhile( new TryObject( "hello", ThrowException, true ) );
function TryObject( value, throwFunction, result ) {
  this.thrower=throwFunction
}
function ThrowException() { return TryInWhile(1); }
function TryInWhile( object ) {
    try {
      object.thrower()
    } catch ( e ) {
    }  
}
