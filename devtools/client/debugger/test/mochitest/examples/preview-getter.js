const obj = {
  propA: 1,
  propB: { arr: [1,2,3,4,5] },
  get getPropA() {
    this.propA;
    return this.propA;
  },
  get getPropB() {
    return this.propB;
  }
}

function funcA() {
  obj.getPropA;
}