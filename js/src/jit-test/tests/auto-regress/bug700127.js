// Binary: cache/js-dbg-64-921e1db5cf11-linux
// Flags: -m -n -a
//

function addThis() {}
function Integer( value ) {
  try {
    checkValue( value )
  } catch (e) {  }
}
function checkValue( value ) {
  if ( addThis() != value || value )
        throw value='foo';
  return value;
}
Integer( 3 );
Integer( NaN );
