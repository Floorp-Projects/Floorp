/*
 * The value class from which all other values derive.
 */

#include "NodeFactory.h"

namespace esc {
namespace v1 {

	std::vector<Node*>* NodeFactory::roots;

	int NodeFactory::main(int argc, char* argv[]) {

		NodeFactory::init();
		
		NodeFactory::Identifier("a");
		NodeFactory::QualifiedIdentifier(
			NodeFactory::Identifier("public"),
			NodeFactory::Identifier("b"));
		NodeFactory::LiteralNull();
		NodeFactory::LiteralBoolean((bool)1);
		NodeFactory::LiteralArray(
			NodeFactory::List(
				NULL,
				NodeFactory::LiteralNumber("one")));
		NodeFactory::LiteralField(
			NodeFactory::Identifier("a"),
			NodeFactory::LiteralBoolean((bool)1));
		NodeFactory::LiteralNumber("3.1415");
		NodeFactory::LiteralObject(
			NodeFactory::List(
				NULL,
				NodeFactory::LiteralField(
					NodeFactory::Identifier("a"),
					NodeFactory::LiteralBoolean((bool)1))));

		
		return 0;
	}
}
}

/*
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved.
 */
