// |reftest| error:SyntaxError module

async () => class { x = await };
