function parseAsModule(source) {
    return Reflect.parse(source, {
        target: "module",
        line: 0x7FFFFFFF + 1,
    });
}
parseAsModule(`
    import {a} from "";
    export {a};
`);
