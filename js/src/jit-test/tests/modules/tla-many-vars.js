a = [];
for (i = 0; i < 1000; ++i) {
    a.push("x" + i);
}

// Sync
parseModule(`
    let ${a.join(",")};
    `);

// Async
parseModule(`
    let ${a.join(",")};
    await 1;
    `);
