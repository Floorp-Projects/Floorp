// |jit-test| error: TypeError
function Integer( value, exception ) {
  try {  } catch ( e ) {  }
  new (value = this)( this.value );
  if ( Math.floor(value) != value || isNaN(value) ) {  }
}
new Integer( 3, false );
