var test = `
class test {
    constructor() { }
    static constructor() { }
}
`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === "function")
    reportCompare(0,0,"OK");
