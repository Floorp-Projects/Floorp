// |jit-test| error:Error

const v0 = `
    function F1() {
        const v11 = registerModule("module1", parseModule(\`import {} from "module2";
                                                            import {} from "module3";\`));
        const v13 = "await 1;";
        drainJobQueue();
        registerModule("module2", parseModule(v13));
        registerModule("module3", parseModule(v0));
        moduleLink(v11);
        moduleEvaluate(v11);
    }
    F1();
`;
eval(v0);
