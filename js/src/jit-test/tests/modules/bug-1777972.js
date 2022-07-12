let a = parseModule(`throw new Error`);
moduleLink(a);
moduleEvaluate(a).catch(e => {});
moduleEvaluate(a).catch(e => {});
