/* -*- Mode: java; tab-width: 8 -*-
 * Copyright © 1998 Netscape Communications Corporation,
 * All Rights Reserved.
 */

import java.util.*;

public class FunctionNode extends Node {

    public FunctionNode(String name, Node left, Node right) {
        super(TokenStream.FUNCTION, left, right, name);
    }

    public String getFunctionName() {
        return getString();
    }

}
