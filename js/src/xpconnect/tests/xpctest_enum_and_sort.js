function enum_and_sort(o)  {
    var all = [];
    for(n in o) all.push(n);
    all.sort();
    for(n in all) print(all[n]);
    print("total count: "+all.length);
}


print("enum_and_sort...\n");
print(enum_and_sort);
print("");

print("enum_and_sort(Components.results)...\n");
enum_and_sort(Components.results);
print("");

print("enum_and_sort(Components.interfaces)...\n");
enum_and_sort(Components.interfaces);
print("");

print("enum_and_sort(Components.classes)...\n");
enum_and_sort(Components.classes);
print("");

print("enum_and_sort(Components.classesByID)...\n");
enum_and_sort(Components.classesByID);
print("");
