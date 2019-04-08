export default function root() {
  function someHelper(){
    console.log("pause here", root, Thing);
  }

  class Thing {}

  someHelper();
}
