async function thing(inc) {
  return new Promise(resolve => {
    setTimeout(resolve, 10);
  });
}

async function main() {
  await thing(1);
  await thing(2);
}
