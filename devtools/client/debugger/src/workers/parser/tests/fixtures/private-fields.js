class MyClass {
  constructor(secret, ...rest) {
    const self = this;
    this.#secret = secret;
    self.#restParams = rest;
  }

  #secret;
  #restParams;
  #salt = "bloup";
  creationDate = new Date();

  #getSalt() {
    return this.#salt;
  }

  debug() {
    const self = this;
    const creationDate = this.creationDate;
    const secret = this.#secret;
    const salt = self.#getSalt();
    return `${creationDate}|${salt}|${secret}`;
  }
}
