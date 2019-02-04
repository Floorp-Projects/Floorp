# Utilities to modify JSON encoded AST
#
# All functions raise exception on unexpected case, to avoid overlooking AST
# change.

def assert_tagged_tuple(obj):
    """
    Assert that the object is a tagged tuple

    :param obj (dict)
           The tagged tuple
    """
    type_ = obj['@TYPE']
    if type_ != 'tagged tuple':
        raise Exception('expected a tagged tuple, got {}'.format(type_))

def assert_list(obj):
    """
    Assert that the object is a list

    :param obj (dict)
           The list
    """
    type_ = obj['@TYPE']
    if type_ != 'list':
        raise Exception('expected a list, got {}'.format(type_))

def assert_interface(obj, expected_name):
    """
    Assert that the object is a tagged tuple for given interface

    :param obj (dict)
           The tagged tuple
    :param expected_name (string)
           The name of the interface
    """
    assert_tagged_tuple(obj)
    actual_name = obj['@INTERFACE']
    if actual_name != expected_name:
        raise Exception('expected {}, got {}'.format(expected_name, actual_name))

def get_field(obj, name):
    """
    Returns the field of the tagged tuple.

    :param obj (dict)
           The tagged tuple
    :param name (string)
           The name of the field to get
    :return (dict)
            The field value
    :raises Exception
            If there's no such field
    """
    assert_tagged_tuple(obj)
    fields = obj['@FIELDS']
    for field in fields:
        if field['@FIELD_NAME'] == name:
            return field['@FIELD_VALUE']
    raise Exception('No such field: {}'.format(name))

def replace_field(obj, name, value):
    """
    Replaces the field of the tagged tuple.

    :param obj (dict)
           the tagged tuple
    :param name (string)
           the name of the field to replace
    :param value (dict)
           the value of the field
    :raises Exception
            If there's no such field
    """
    assert_tagged_tuple(obj)
    fields = obj['@FIELDS']
    for field in fields:
        if field['@FIELD_NAME'] == name:
            field['@FIELD_VALUE'] = value
            return
    raise Exception('No such field: {}'.format(name))

def remove_field(obj, name):
    """
    Removes the field from the tagged tuple

    :param obj (dict)
           the tagged tuple
    :param name (string)
           the name of the field to remove
    :raises Exception
            If there's no such field
    """
    assert_tagged_tuple(obj)
    i = 0
    fields = obj['@FIELDS']
    for field in fields:
        if field['@FIELD_NAME'] == name:
            del fields[i]
            return
        i += 1
    raise Exception('No such field: {}'.format(name))

def get_element(obj, i):
    """
    Returns the element of the list.

    :param obj (dict)
           The list
    :param i (int)
           The indef of the element to get
    :return (dict)
            The element value
    :raises Exception
            On out of bound access
    """
    assert_list(obj)
    elements = obj['@VALUE']
    if i >= len(elements):
        raise Exception('Out of bound: {} < {}'.format(i, len(elements)))
    return elements[i]

def replace_element(obj, i, value):
    """
    Replaces the element of the list.

    :param obj (dict)
           the list
    :param i (int)
           The indef of the element to replace
    :param value (dict)
           the value of the element
    :raises Exception
            On out of bound access
    """
    assert_list(obj)
    elements = obj['@VALUE']
    if i >= len(elements):
        raise Exception('Out of bound: {} < {}'.format(i, len(elements)))
    elements[i] = value

def append_element(obj, value):
    """
    Appends the element to the list.

    :param obj (dict)
           the list
    :param value (dict)
           the value to be added to the list
    """
    assert_list(obj)
    elements = obj['@VALUE']
    elements.append(value)

def remove_element(obj, i):
    """
    Removes the element from the list

    :param obj (dict)
           the list
    :param i (int)
           The indef of the element to remove
    :raises Exception
            On out of bound access
    """
    assert_list(obj)
    elements = obj['@VALUE']
    if i >= len(elements):
        raise Exception('Out of bound: {} < {}'.format(i, len(elements)))
    del elements[i]
