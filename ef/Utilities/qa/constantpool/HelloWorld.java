class HelloWorld implements testInterface {

	static long field = 255L;
	
	long sum;

	HelloWorld() {
		
		sum = field + 245L;

	}
		

	public void doSomething(String line) {
		
		System.out.println(line);

	}

	void test(long field){
		
		double l = 5000.0D;
		int i = 1234567890;
		float j = 1234.0F;
		long k = 5000L;
		

		int m = i + 1;
		float n = j + 1000.0F;
		long o = k + field;	
		double p = l + 300.0D;	
		
	}

	public static void main(String[] args) {
		
		long field = 255L;

		HelloWorld a = new HelloWorld();
		a.test(field);

	}
	
}