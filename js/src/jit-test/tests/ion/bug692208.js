// |jit-test| error: ReferenceError
SwitchTest( 0, 126 );
function SwitchTest( input, expect ) {
  switch ( input ) {
  case (7):
  default:
    result += 32;
  case 4:
  }
}
