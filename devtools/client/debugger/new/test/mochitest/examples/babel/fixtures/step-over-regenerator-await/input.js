var fn = async function fn() {
  console.log("pause here");

  await doAsync();

  console.log("stopped here");
};

function doAsync() {
  return Promise.resolve();
}

export default function root() {
  fn();
}
