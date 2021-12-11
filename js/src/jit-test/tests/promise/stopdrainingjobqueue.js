Promise.resolve()
    .then(()=>quit(0));
Promise.resolve()
    .then(()=>crash("Must not run any more promise jobs after quitting"));