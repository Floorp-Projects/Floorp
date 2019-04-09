const frameworkStars = {
  angular: 40779,
  react: 111576,
  vue: 114358,
};

const container = new Container();
container.module("sum", (...args) => args.reduce((s, c) => s + c, 0));

container.get("sum",
  (sum) => sum(Object.values(frameworkStars)));
