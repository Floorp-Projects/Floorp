function ExprArray(n,v) {
    for (var i = 0; i < n; i++) {
	this[i] = "" + v;
    }
}

function perfect(n) {
    print("The perfect numbers up to " +  n + " are:");
    var sumOfDivisors = new ExprArray(n+1,1);
    for (var divisor = 2; divisor <= n; divisor++) {
	for (var j = divisor + divisor; j <= n; j += divisor) {
	    sumOfDivisors[j] += " + " + divisor;
	}
	if (eval(sumOfDivisors[divisor]) == divisor) {
	    print("" + divisor + " = " + sumOfDivisors[divisor]);
	}
    }
    print("That's all.");
}


print("\nA number is 'perfect' if it is equal to the sum of its");
print("divisors (excluding itself).\n");
perfect(500);

