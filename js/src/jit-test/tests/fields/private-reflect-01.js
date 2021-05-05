function rp(x) {
  return Reflect.parse(x);
};

rp(`(
    class {
      static #m = 'test262';
    }
  )`);

rp(`(
    class {
      #m = 'test262';
    }
  )`);

rp(`(
    class {
      #m = 'test262';
      gm() {
          this.#m++;
          this.#m--;
          this.#m?.x;
          o[this.#m];
          return this.#m;
      }
      sm() {
          this.#m = 12;
      }
    }
  )`);

rp(`(
    class {
      static #m = 'test262';
      static gm() {
        this.#m++;
        this.#m--;
        this.#m?.x;
        o[this.#m];
        return this.#m;
      }
      static sm() {
          this.#m = 12;
      }
    }
  )`);
