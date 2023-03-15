function a() {}

function stuff() {
  a(); a();
  a(); a();
  debugger
}
function funcWithMultipleBreakableColumns() {
  const items = [1,2];for(let i=0;i<items.length;i++){items[i]=items[i]+1}
}